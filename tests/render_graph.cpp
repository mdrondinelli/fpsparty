#include "render_graph/schedule.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace fpsparty;
using namespace fpsparty::render_graph;

namespace {

constexpr auto compute_read = Access{
  .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
  .access_mask = graphics::Access_flag_bits::shader_sampled_read,
};
constexpr auto compute_write = Access{
  .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
  .access_mask = graphics::Access_flag_bits::shader_storage_write,
};
constexpr auto indirect_read = Access{
  .stage_mask = graphics::Pipeline_stage_flag_bits::draw_indirect,
  .access_mask = graphics::Access_flag_bits::indirect_command_read,
};
constexpr auto fragment_read = Access{
  .stage_mask = graphics::Pipeline_stage_flag_bits::fragment_shader,
  .access_mask = graphics::Access_flag_bits::shader_sampled_read,
};
constexpr auto color_write = Access{
  .stage_mask = graphics::Pipeline_stage_flag_bits::color_attachment_output,
  .access_mask = graphics::Access_flag_bits::color_attachment_write,
};

// Owns the per-pass access vectors so the spans handed to build_schedule
// stay valid.
class Graph_fixture {
public:
  u32 add(std::vector<Scheduled_access> accesses) {
    _accesses.push_back(std::move(accesses));
    return static_cast<u32>(_accesses.size() - 1);
  }

  void set_disjoint(u32 a, u32 b, u32 resource) {
    _disjoint.push_back({.pass_a = a, .pass_b = b, .resource = resource});
  }

  Schedule build() {
    auto passes = std::vector<Scheduled_pass>{};
    passes.reserve(_accesses.size());
    for (auto const &accesses : _accesses) {
      passes.push_back({.accesses = accesses});
    }
    return build_schedule(passes, _disjoint);
  }

private:
  std::vector<std::vector<Scheduled_access>> _accesses{};
  std::vector<Disjoint_access> _disjoint{};
};

u64 stages(Access access) { return static_cast<u64>(access.stage_mask); }
u64 accesses(Access access) { return static_cast<u64>(access.access_mask); }

} // namespace

TEST_CASE("passes with no shared resources all land in layer 0") {
  auto fixture = Graph_fixture{};
  fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  fixture.add({{.resource = 1, .access = compute_write, .is_write = true}});
  fixture.add({{.resource = 2, .access = color_write, .is_write = true}});
  auto const schedule = fixture.build();
  CHECK(schedule.levels == std::vector<u32>{0, 0, 0});
  CHECK(schedule.barriers.empty());
}

TEST_CASE("read after write orders the reader into the next layer") {
  auto fixture = Graph_fixture{};
  fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  fixture.add({{.resource = 0, .access = compute_read, .is_write = false}});
  auto const schedule = fixture.build();
  REQUIRE(schedule.levels == std::vector<u32>{0, 1});
  REQUIRE(schedule.barriers.size() == 1);
  CHECK(schedule.barriers[0].src.access_mask == compute_write.access_mask);
  CHECK(schedule.barriers[0].dst.access_mask == compute_read.access_mask);
}

TEST_CASE("write after write carries the write bit in dst") {
  auto fixture = Graph_fixture{};
  fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  auto const schedule = fixture.build();
  REQUIRE(schedule.levels == std::vector<u32>{0, 1});
  REQUIRE(schedule.barriers.size() == 1);
  CHECK(schedule.barriers[0].dst.access_mask == compute_write.access_mask);
}

TEST_CASE("write after read is execution only") {
  auto fixture = Graph_fixture{};
  fixture.add({{.resource = 0, .access = compute_read, .is_write = false}});
  fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  auto const schedule = fixture.build();
  REQUIRE(schedule.levels == std::vector<u32>{0, 1});
  REQUIRE(schedule.barriers.size() == 1);
  CHECK(stages(schedule.barriers[0].src) == stages(compute_read));
  CHECK(stages(schedule.barriers[0].dst) == stages(compute_write));
  // Execution dependency only -- asking for cache work here would be
  // wasted.
  CHECK(accesses(schedule.barriers[0].src) == 0);
  CHECK(accesses(schedule.barriers[0].dst) == 0);
}

TEST_CASE("a node reading and writing one resource does not depend on itself") {
  auto fixture = Graph_fixture{};
  fixture.add({
    {.resource = 0, .access = compute_read, .is_write = false},
    {.resource = 0, .access = compute_write, .is_write = true},
  });
  auto const schedule = fixture.build();
  CHECK(schedule.levels == std::vector<u32>{0});
  CHECK(schedule.barriers.empty());
}

TEST_CASE("a read already covered by an earlier boundary adds no bits") {
  auto fixture = Graph_fixture{};
  // Colour write, then a chain that pushes a second reader of resource 0
  // out to layer 2. Its hazard is already satisfied by the boundary before
  // layer 1, so the later boundary must carry no colour bits.
  fixture.add({{.resource = 0, .access = color_write, .is_write = true}});
  fixture.add({
    {.resource = 0, .access = compute_read, .is_write = false},
    {.resource = 1, .access = compute_write, .is_write = true},
  });
  fixture.add({
    {.resource = 1, .access = compute_read, .is_write = false},
    {.resource = 0, .access = compute_read, .is_write = false},
  });
  auto const schedule = fixture.build();
  REQUIRE(schedule.levels == std::vector<u32>{0, 1, 2});
  REQUIRE(schedule.barriers.size() == 2);
  CHECK(stages(schedule.barriers[0].src) == stages(color_write));
  CHECK(stages(schedule.barriers[1].src) == stages(compute_write));
  CHECK(accesses(schedule.barriers[1].src) == accesses(compute_write));
}

TEST_CASE("coverage is not unioned across separate barriers") {
  auto fixture = Graph_fixture{};
  // Two earlier boundaries each flush resource 0 to one half of what the
  // final reader needs. Neither covers it alone, so it must contribute.
  fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  fixture.add({
    {.resource = 0, .access = compute_read, .is_write = false},
    {.resource = 1, .access = compute_write, .is_write = true},
  });
  fixture.add({
    {.resource = 1, .access = indirect_read, .is_write = false},
    {.resource = 2, .access = compute_write, .is_write = true},
  });
  auto const combined = Access{
    .stage_mask = compute_read.stage_mask | indirect_read.stage_mask,
    .access_mask = compute_read.access_mask | indirect_read.access_mask,
  };
  fixture.add({
    {.resource = 2, .access = compute_read, .is_write = false},
    {.resource = 0, .access = combined, .is_write = false},
  });
  auto const schedule = fixture.build();
  REQUIRE(schedule.levels == std::vector<u32>{0, 1, 2, 3});
  REQUIRE(schedule.barriers.size() == 3);
  CHECK(accesses(schedule.barriers[2].dst) == accesses(combined));
  CHECK(stages(schedule.barriers[2].dst) == stages(combined));
}

TEST_CASE("a disjoint declaration drops the edge and merges the layer") {
  auto fixture = Graph_fixture{};
  auto const sun =
    fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  auto const sky =
    fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  SECTION("without the declaration they serialize") {
    auto const schedule = fixture.build();
    CHECK(schedule.levels == std::vector<u32>{0, 1});
    CHECK(schedule.barriers.size() == 1);
  }
  SECTION("with it they share a layer") {
    fixture.set_disjoint(sun, sky, 0);
    auto const schedule = fixture.build();
    CHECK(schedule.levels == std::vector<u32>{0, 0});
    CHECK(schedule.barriers.empty());
  }
}

TEST_CASE("a chain of dependent passes gets one layer each") {
  auto fixture = Graph_fixture{};
  fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  for (auto i = 0; i != 4; ++i) {
    fixture.add({
      {.resource = static_cast<u32>(i), .access = compute_read},
      {.resource = static_cast<u32>(i + 1),
       .access = compute_write,
       .is_write = true},
    });
  }
  auto const schedule = fixture.build();
  CHECK(schedule.levels == std::vector<u32>{0, 1, 2, 3, 4});
  CHECK(schedule.barriers.size() == 4);
}

TEST_CASE("an empty graph schedules nothing") {
  auto fixture = Graph_fixture{};
  auto const schedule = fixture.build();
  CHECK(schedule.levels.empty());
  CHECK(schedule.barriers.empty());
}

TEST_CASE("a disjoint writer does not hide an earlier conflicting writer") {
  auto fixture = Graph_fixture{};
  // Three accesses to one resource: P0 and P2 touch the same region, P1 a
  // different one. Both disjoint promises are true, but P2 still depends
  // on P0 and nothing has ordered them.
  auto const p0 =
    fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  auto const p1 =
    fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  auto const p2 =
    fixture.add({{.resource = 0, .access = compute_read, .is_write = false}});
  fixture.set_disjoint(p0, p1, 0);
  fixture.set_disjoint(p1, p2, 0);
  auto const schedule = fixture.build();
  CHECK(schedule.levels == std::vector<u32>{0, 0, 1});
  REQUIRE(schedule.barriers.size() == 1);
  CHECK(accesses(schedule.barriers[0].src) == accesses(compute_write));
  CHECK(accesses(schedule.barriers[0].dst) == accesses(compute_read));
}

TEST_CASE("a pass that reads and writes keeps both scopes for later writers") {
  auto fixture = Graph_fixture{};
  // Reading in one stage and writing in another: a later writer needs a
  // memory dependency on the write and an execution dependency on the
  // read, so both stages belong in src.
  fixture.add({
    {.resource = 0, .access = fragment_read, .is_write = false},
    {.resource = 0, .access = compute_write, .is_write = true},
  });
  fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
  auto const schedule = fixture.build();
  REQUIRE(schedule.levels == std::vector<u32>{0, 1});
  REQUIRE(schedule.barriers.size() == 1);
  CHECK(
    stages(schedule.barriers[0].src) ==
    (stages(fragment_read) | stages(compute_write)));
  CHECK(accesses(schedule.barriers[0].src) == accesses(compute_write));
}

TEST_CASE("declaration order within a pass does not change synchronization") {
  auto const build = [](bool read_first) {
    auto fixture = Graph_fixture{};
    auto entries = std::vector<Scheduled_access>{
      {.resource = 0, .access = fragment_read, .is_write = false},
      {.resource = 0, .access = compute_write, .is_write = true},
    };
    if (!read_first) {
      std::swap(entries[0], entries[1]);
    }
    fixture.add(std::move(entries));
    fixture.add({{.resource = 0, .access = compute_write, .is_write = true}});
    return fixture.build();
  };
  auto const read_first = build(true);
  auto const write_first = build(false);
  REQUIRE(read_first.barriers.size() == write_first.barriers.size());
  CHECK(read_first.levels == write_first.levels);
  CHECK(
    stages(read_first.barriers[0].src) == stages(write_first.barriers[0].src));
  CHECK(
    accesses(read_first.barriers[0].src) ==
    accesses(write_first.barriers[0].src));
}
