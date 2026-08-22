#include "render_graph/schedule.hpp"
#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <unordered_map>

namespace fpsparty::render_graph {

namespace {

// An Access is a set of stage bits and access bits, so one barrier's
// scope satisfies a requirement exactly when it is a superset.
bool covers(Access outer, Access inner) noexcept {
  return static_cast<u64>(outer.stage_mask & inner.stage_mask) ==
           static_cast<u64>(inner.stage_mask) &&
         static_cast<u64>(outer.access_mask & inner.access_mask) ==
           static_cast<u64>(inner.access_mask);
}

bool is_empty(Access access) noexcept {
  return static_cast<u64>(access.stage_mask) == 0 &&
         static_cast<u64>(access.access_mask) == 0;
}

// The scope of an ordering requirement that needs no cache work, only for
// one stage to finish before another starts. Dropping the access bits is
// what keeps a write-after-read from asking for a flush.
Access execution_scope(Access access) noexcept {
  return {.stage_mask = access.stage_mask, .access_mask = {}};
}

// The caller's disjointness promises. Holds the one fact that they are
// symmetric, so no caller has to remember to check both orders.
class Disjoint_set {
public:
  explicit Disjoint_set(
    std::span<Disjoint_access const> declarations) noexcept
      : _declarations{declarations} {}

  bool contains(u32 pass_a, u32 pass_b, u32 resource) const noexcept {
    return std::ranges::any_of(
      _declarations, [&](Disjoint_access const &declaration) {
        return declaration.resource == resource &&
               ((declaration.pass_a == pass_a &&
                 declaration.pass_b == pass_b) ||
                (declaration.pass_a == pass_b &&
                 declaration.pass_b == pass_a));
      });
  }

private:
  std::span<Disjoint_access const> _declarations;
};

// One ordering requirement between two passes.
struct Hazard {
  u32 producer;
  u32 consumer;
  Access src;
  Access dst;

  // Only constructible pointing backwards. assign_levels treats a
  // producer's level as final when it reaches the consumer, which holds
  // only because hazards are derived from state describing earlier
  // passes.
  Hazard(u32 producer, u32 consumer, Access src, Access dst) noexcept
      : producer{producer}, consumer{consumer}, src{src}, dst{dst} {
    assert(producer < consumer);
  }
};

// A pass's access to a resource that no later write has superseded yet.
struct Outstanding_access {
  u32 pass{};
  Access access{};
};

// Per resource, everything still outstanding. A write supersedes only
// what it is not disjoint from, so writing one region leaves accesses to
// other regions of the same resource outstanding.
struct Resource_state {
  std::vector<Outstanding_access> writers{};
  std::vector<Outstanding_access> readers{};
};

// One pass's whole relationship with one resource. The optionals make
// "read in this scope" and "did not read" the same field, so the two
// cannot disagree.
struct Pass_access {
  u32 resource{};
  std::optional<Access> read{};
  std::optional<Access> write{};
};

void merge_into(std::optional<Access> &target, Access access) noexcept {
  target = target ? *target | access : access;
}

// Collapses a pass's declarations to one entry per resource, so that the
// order it declared them in cannot change the schedule.
std::vector<Pass_access>
merge_accesses(std::span<Scheduled_access const> entries) {
  auto merged = std::vector<Pass_access>{};
  for (auto const &entry : entries) {
    auto found =
      std::ranges::find(merged, entry.resource, &Pass_access::resource);
    if (found == merged.end()) {
      merged.push_back({.resource = entry.resource});
      found = std::prev(merged.end());
    }
    merge_into(entry.is_write ? found->write : found->read, entry.access);
  }
  return merged;
}

// Requirements between this pass and the outstanding accesses of earlier
// ones. states must not yet include this pass, which is what keeps a pass
// that both reads and writes a resource from depending on itself.
void collect_hazards(
  u32 pass,
  std::span<Pass_access const> merged,
  std::unordered_map<u32, Resource_state> const &states,
  Disjoint_set const &disjoint,
  std::vector<Hazard> &hazards) {
  for (auto const &access : merged) {
    auto const found = states.find(access.resource);
    if (found == states.end()) {
      continue;
    }
    auto const &state = found->second;
    for (auto const &writer : state.writers) {
      if (disjoint.contains(writer.pass, pass, access.resource)) {
        continue;
      }
      // Read after write and write after write both need the producer's
      // writes made available here.
      if (access.read) {
        hazards.emplace_back(writer.pass, pass, writer.access, *access.read);
      }
      if (access.write) {
        hazards.emplace_back(writer.pass, pass, writer.access, *access.write);
      }
    }
    if (!access.write) {
      continue;
    }
    for (auto const &reader : state.readers) {
      if (disjoint.contains(reader.pass, pass, access.resource)) {
        continue;
      }
      // Write after read only has to wait for the read to finish.
      hazards.emplace_back(
        reader.pass,
        pass,
        execution_scope(reader.access),
        execution_scope(*access.write));
    }
  }
}

// Publishes this pass's accesses, retiring whatever its writes supersede.
void apply_accesses(
  u32 pass,
  std::span<Pass_access const> merged,
  Disjoint_set const &disjoint,
  std::unordered_map<u32, Resource_state> &states) {
  for (auto const &access : merged) {
    auto &state = states[access.resource];
    if (access.write) {
      // Anything this write supersedes stays reachable through it, so
      // only the disjoint accesses need to remain outstanding.
      auto const superseded = [&](Outstanding_access const &other) {
        return !disjoint.contains(other.pass, pass, access.resource);
      };
      std::erase_if(state.writers, superseded);
      std::erase_if(state.readers, superseded);
      state.writers.push_back({.pass = pass, .access = *access.write});
    }
    if (access.read) {
      state.readers.push_back({.pass = pass, .access = *access.read});
    }
  }
}

// Earliest layer each pass can occupy. Relies on Hazard's backwards
// invariant: every producer's level is final by the time a hazard naming
// it is read.
std::vector<u32>
assign_levels(std::span<Hazard const> hazards, u32 pass_count) {
  auto levels = std::vector<u32>(pass_count, 0);
  for (auto const &hazard : hazards) {
    levels[hazard.consumer] =
      std::max(levels[hazard.consumer], levels[hazard.producer] + 1);
  }
  return levels;
}

// One barrier per layer boundary, where the result's index b is the
// barrier emitted before layer b + 1. A hazard is carried by the boundary
// immediately before its consumer; because the barrier is global, that
// single placement also orders producers several layers back.
std::vector<Barrier_record>
build_barriers(std::span<Hazard const> hazards, std::span<u32 const> levels) {
  if (levels.empty()) {
    return {};
  }
  auto layer_count = u32{1};
  for (auto const level : levels) {
    layer_count = std::max(layer_count, level + 1);
  }
  auto barriers = std::vector<Barrier_record>(layer_count - 1);
  for (auto boundary = u32{}; boundary != barriers.size(); ++boundary) {
    for (auto const &hazard : hazards) {
      if (levels[hazard.consumer] != boundary + 1) {
        continue;
      }
      // Already satisfied if a boundary between the producer's layer and
      // this one carries both scopes. Each candidate is tested on its own
      // -- a union across several would claim coverage that no single
      // dependency provided.
      auto const candidates = std::ranges::subrange{
        std::next(barriers.begin(), levels[hazard.producer]),
        std::next(barriers.begin(), boundary),
      };
      if (std::ranges::any_of(candidates, [&](Barrier_record const &earlier) {
            return covers(earlier.src, hazard.src) &&
                   covers(earlier.dst, hazard.dst);
          })) {
        continue;
      }
      barriers[boundary].src = barriers[boundary].src | hazard.src;
      barriers[boundary].dst = barriers[boundary].dst | hazard.dst;
    }
  }
  return barriers;
}

} // namespace

Schedule build_schedule(
  std::span<Scheduled_pass const> passes,
  std::span<Disjoint_access const> disjoint) {
  auto const pass_count = static_cast<u32>(passes.size());
  auto const disjoint_set = Disjoint_set{disjoint};
  auto hazards = std::vector<Hazard>{};
  auto states = std::unordered_map<u32, Resource_state>{};
  for (auto pass = u32{}; pass != pass_count; ++pass) {
    // Merge, then compare against earlier passes, then publish. Keeping
    // the last two apart is what makes a pass's own accesses invisible to
    // its own hazards.
    auto const merged = merge_accesses(passes[pass].accesses);
    collect_hazards(pass, merged, states, disjoint_set, hazards);
    apply_accesses(pass, merged, disjoint_set, states);
  }
  auto levels = assign_levels(hazards, pass_count);
  auto barriers = build_barriers(hazards, levels);
  return {.levels = std::move(levels), .barriers = std::move(barriers)};
}

bool barrier_record_is_empty(Barrier_record const &barrier) noexcept {
  return is_empty(barrier.src) && is_empty(barrier.dst);
}

} // namespace fpsparty::render_graph
