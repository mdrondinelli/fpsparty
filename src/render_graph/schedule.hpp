#ifndef FPSPARTY_RENDER_GRAPH_SCHEDULE_HPP
#define FPSPARTY_RENDER_GRAPH_SCHEDULE_HPP

#include "int.hpp"
#include "render_graph/access.hpp"
#include <span>
#include <vector>

namespace fpsparty::render_graph {

// One resource access declared by one pass. resource is a dense id; two
// accesses share an id only if they touch the same memory.
struct Scheduled_access {
  u32 resource{};
  Access access{};
  bool is_write{};
};

// One pass's accesses, in declaration order.
struct Scheduled_pass {
  std::span<Scheduled_access const> accesses{};
};

// A caller's unchecked promise that two passes touch disjoint parts of one
// resource, so no hazard between them needs ordering. Symmetric.
struct Disjoint_access {
  u32 pass_a{};
  u32 pass_b{};
  u32 resource{};
};

// A global memory barrier: all prior commands limited to src, ordered
// against all later commands limited to dst.
struct Barrier_record {
  Access src{};
  Access dst{};
};

struct Schedule {
  // Layer of each pass. Passes sharing a layer have no hazards between
  // them and may be recorded in any order.
  std::vector<u32> levels{};
  // barriers[b] goes before layer b + 1, so size is layer count - 1. A
  // barrier with empty masks needs no command.
  std::vector<Barrier_record> barriers{};
};

// Derives RAW/WAW/WAR hazards from the declarations, assigns each pass to
// the earliest layer its dependencies allow, and builds one barrier per
// layer boundary. Pass order defines which side of a conflicting pair is
// the producer; it does not constrain execution otherwise.
Schedule build_schedule(
  std::span<Scheduled_pass const> passes,
  std::span<Disjoint_access const> disjoint);

// True when a boundary carried no uncovered hazard, so no command is
// needed for it.
bool barrier_record_is_empty(Barrier_record const &barrier) noexcept;

} // namespace fpsparty::render_graph

#endif
