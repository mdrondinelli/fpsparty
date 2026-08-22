#include "render_graph/schedule.hpp"
#include <algorithm>
#include <unordered_map>

namespace fpsparty::render_graph {

namespace {

bool contains(Access outer, Access inner) noexcept {
  auto const stage = static_cast<u64>(outer.stage_mask & inner.stage_mask) ==
                     static_cast<u64>(inner.stage_mask);
  auto const access = static_cast<u64>(outer.access_mask & inner.access_mask) ==
                      static_cast<u64>(inner.access_mask);
  return stage && access;
}

bool is_empty(Access access) noexcept {
  return static_cast<u64>(access.stage_mask) == 0 &&
         static_cast<u64>(access.access_mask) == 0;
}

// One ordering requirement between two passes. producer is always the
// lower index, so hazards form a DAG with pass order as a topological
// order.
struct Hazard {
  u32 producer{};
  u32 consumer{};
  Access src{};
  Access dst{};
};

// Per resource, the accesses still outstanding. A write only supersedes
// the accesses it is not disjoint from, so a write to one region leaves
// accesses to other regions of the same resource outstanding.
struct Resource_state {
  std::vector<std::pair<u32, Access>> writers{};
  std::vector<std::pair<u32, Access>> readers{};
};

// One pass's accesses to one resource, merged so that declaration order
// within the pass cannot change the result.
struct Pass_access {
  u32 resource{};
  Access read{};
  Access write{};
  bool has_read{};
  bool has_write{};
};

} // namespace

Schedule build_schedule(
  std::span<Scheduled_pass const> passes,
  std::span<Disjoint_access const> disjoint) {
  auto const pass_count = static_cast<u32>(passes.size());
  auto schedule = Schedule{.levels = std::vector<u32>(pass_count, 0)};
  if (pass_count == 0) {
    return schedule;
  }

  auto const is_disjoint = [&](u32 a, u32 b, u32 resource) {
    return std::ranges::any_of(disjoint, [&](Disjoint_access const &d) {
      return d.resource == resource &&
             ((d.pass_a == a && d.pass_b == b) ||
              (d.pass_a == b && d.pass_b == a));
    });
  };

  // Hazards come out grouped by consumer in ascending order, which the
  // level assignment below relies on.
  auto hazards = std::vector<Hazard>{};
  auto states = std::unordered_map<u32, Resource_state>{};
  auto merged = std::vector<Pass_access>{};
  for (auto pass = u32{}; pass != pass_count; ++pass) {
    // Merge first: a pass that reads and writes one resource must end up
    // with both scopes regardless of which it declared first.
    merged.clear();
    for (auto const &entry : passes[pass].accesses) {
      auto const existing = std::ranges::find(
        merged, entry.resource, &Pass_access::resource);
      auto &access = existing != merged.end()
                       ? *existing
                       : merged.emplace_back(Pass_access{
                           .resource = entry.resource,
                         });
      if (entry.is_write) {
        access.write =
          access.has_write ? access.write | entry.access : entry.access;
        access.has_write = true;
      } else {
        access.read =
          access.has_read ? access.read | entry.access : entry.access;
        access.has_read = true;
      }
    }

    // Decide every hazard against state from earlier passes, so a pass
    // that both reads and writes one resource doesn't depend on itself.
    for (auto const &access : merged) {
      auto const it = states.find(access.resource);
      if (it == states.end()) {
        continue;
      }
      auto const &state = it->second;
      for (auto const &[writer, writer_access] : state.writers) {
        if (is_disjoint(writer, pass, access.resource)) {
          continue;
        }
        // Read after write, and write after write: both need the
        // producer's writes made available to the consumer.
        if (access.has_read) {
          hazards.push_back({
            .producer = writer,
            .consumer = pass,
            .src = writer_access,
            .dst = access.read,
          });
        }
        if (access.has_write) {
          hazards.push_back({
            .producer = writer,
            .consumer = pass,
            .src = writer_access,
            .dst = access.write,
          });
        }
      }
      if (!access.has_write) {
        continue;
      }
      // Write after read only needs execution ordering, so the access
      // masks stay empty and no cache work is asked for.
      for (auto const &[reader, reader_access] : state.readers) {
        if (is_disjoint(reader, pass, access.resource)) {
          continue;
        }
        hazards.push_back({
          .producer = reader,
          .consumer = pass,
          .src = {.stage_mask = reader_access.stage_mask, .access_mask = {}},
          .dst = {.stage_mask = access.write.stage_mask, .access_mask = {}},
        });
      }
    }

    for (auto const &access : merged) {
      auto &state = states[access.resource];
      if (access.has_write) {
        // Anything this write supersedes is now reachable through it, so
        // only the disjoint accesses stay outstanding.
        auto const superseded = [&](std::pair<u32, Access> const &other) {
          return !is_disjoint(other.first, pass, access.resource);
        };
        std::erase_if(state.writers, superseded);
        std::erase_if(state.readers, superseded);
        state.writers.push_back({pass, access.write});
      }
      if (access.has_read) {
        state.readers.push_back({pass, access.read});
      }
    }
  }

  // Earliest layer each pass can sit in. Every producer has a lower index
  // and so a final level by the time its consumer is reached.
  auto layer_count = u32{1};
  for (auto const &hazard : hazards) {
    auto &level = schedule.levels[hazard.consumer];
    level = std::max(level, schedule.levels[hazard.producer] + 1);
    layer_count = std::max(layer_count, level + 1);
  }

  // Each hazard is carried by the boundary immediately before its
  // consumer. Because the barrier is global, that one placement also
  // covers producers several layers back.
  schedule.barriers.resize(layer_count - 1);
  for (auto boundary = u32{}; boundary != layer_count - 1; ++boundary) {
    for (auto const &hazard : hazards) {
      if (schedule.levels[hazard.consumer] != boundary + 1) {
        continue;
      }
      // Already satisfied if some boundary after the producer carries both
      // scopes. Test one barrier at a time -- a union across several would
      // wrongly claim coverage of a scope no single dependency provided.
      auto covered = false;
      for (auto earlier = schedule.levels[hazard.producer]; earlier != boundary;
           ++earlier) {
        if (contains(schedule.barriers[earlier].src, hazard.src) &&
            contains(schedule.barriers[earlier].dst, hazard.dst)) {
          covered = true;
          break;
        }
      }
      if (covered) {
        continue;
      }
      schedule.barriers[boundary].src =
        schedule.barriers[boundary].src | hazard.src;
      schedule.barriers[boundary].dst =
        schedule.barriers[boundary].dst | hazard.dst;
    }
  }
  return schedule;
}

bool barrier_record_is_empty(Barrier_record const &barrier) noexcept {
  return is_empty(barrier.src) && is_empty(barrier.dst);
}

} // namespace fpsparty::render_graph
