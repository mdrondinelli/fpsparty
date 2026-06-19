#include "game/grid.hpp"
#include "serial/span_reader.hpp"
#include "serial/span_writer.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

namespace {

namespace game = fpsparty::game;
namespace math = fpsparty::math;
namespace serial = fpsparty::serial;

// A 16^3 grid straddling the origin, so coordinates on every axis are negative,
// zero, and positive. Spans chunks [-2, 1] on each axis (edge_length 4).
auto const straddling_bounds =
  math::ibox3{math::ivec3{-8, -8, -8}, math::ivec3{7, 7, 7}};

game::Grid make_grid() { return game::Grid{{.bounds = straddling_bounds}}; }

} // namespace

TEST_CASE("Grid cell_to_chunk floors toward negative infinity") {
  CHECK(
    game::Grid::cell_to_chunk(math::ivec3{0, 3, 4}) == math::ivec3{0, 0, 1});
  CHECK(
    game::Grid::cell_to_chunk(math::ivec3{-1, -4, -5}) ==
    math::ivec3{-1, -1, -2});
  CHECK(
    game::Grid::cell_to_chunk(math::ivec3{7, 8, -8}) == math::ivec3{1, 2, -2});
}

TEST_CASE("Grid chunk_to_cell returns the chunk's origin cell") {
  CHECK(
    game::Grid::chunk_to_cell(math::ivec3{0, 1, -2}) == math::ivec3{0, 4, -8});
}

TEST_CASE("Grid stores and reports solidity at negative coordinates") {
  auto grid = make_grid();
  grid.set_block({-8, -8, -8}, game::Block::solid);
  grid.set_block({-1, 0, 3}, game::Block::solid);
  CHECK(grid.is_solid({-8, -8, -8}));
  CHECK(grid.is_solid({-1, 0, 3}));
  CHECK_FALSE(grid.is_solid({-7, -8, -8}));
}

TEST_CASE("Grid bounds are inclusive at both corners") {
  auto const grid = make_grid();
  CHECK(grid.bounds_check_cell({-8, -8, -8}));
  CHECK(grid.bounds_check_cell({7, 7, 7}));
  CHECK_FALSE(grid.bounds_check_cell({-9, 0, 0}));
  CHECK_FALSE(grid.bounds_check_cell({8, 0, 0}));
}

TEST_CASE("Grid treats out-of-bounds writes and reads as non-solid") {
  auto grid = make_grid();
  grid.set_block({8, 0, 0}, game::Block::solid);
  CHECK_FALSE(grid.is_solid({8, 0, 0}));
  CHECK_FALSE(grid.is_solid({-9, 0, 0}));
}

TEST_CASE("Grid fill sets every cell in an inclusive box") {
  auto grid = make_grid();
  grid.fill(
    math::ibox3{math::ivec3{-2, -2, -2}, math::ivec3{0, 0, 0}},
    game::Block::solid);
  CHECK(grid.is_solid({-2, -2, -2}));
  CHECK(grid.is_solid({0, 0, 0}));
  CHECK_FALSE(grid.is_solid({1, 0, 0}));
  CHECK_FALSE(grid.is_solid({-3, -2, -2}));
}

TEST_CASE("Grid fill clamps to the grid bounds") {
  auto grid = make_grid();
  grid.fill(
    math::ibox3{math::ivec3{5, 5, 5}, math::ivec3{100, 100, 100}},
    game::Block::solid);
  CHECK(grid.is_solid({7, 7, 7}));
  CHECK(grid.is_solid({5, 5, 5}));
}

TEST_CASE("Grid reports cell and chunk extents spanning negative bounds") {
  auto const grid = make_grid();
  CHECK(grid.get_cell_counts() == math::ivec3{16, 16, 16});
  CHECK(grid.get_chunk_counts() == math::ivec3{4, 4, 4});
  CHECK(grid.get_chunk_bounds().min() == math::ivec3{-2, -2, -2});
  CHECK(grid.get_chunk_bounds().max() == math::ivec3{1, 1, 1});
}

TEST_CASE("Default-constructed grid is empty and iterates no chunks") {
  auto const grid = game::Grid{game::Grid_create_info{}};
  CHECK(grid.empty());
  CHECK_FALSE(grid.bounds_check_cell({0, 0, 0}));
  auto count = 0;
  for ([[maybe_unused]] auto const &chunk : grid.get_chunks()) {
    ++count;
  }
  CHECK(count == 0);
}

TEST_CASE("Grid survives a serialization round-trip") {
  auto original = make_grid();
  original.set_block({-8, -8, -8}, game::Block::solid);
  original.set_block({7, 7, 7}, game::Block::solid);
  auto buffer = std::array<std::byte, 4096>{};
  auto writer = serial::Span_writer{buffer};
  original.dump(writer);
  auto reader = serial::Span_reader{std::span{buffer}.first(writer.offset())};
  auto restored = game::Grid{game::Grid_create_info{}};
  restored.load(reader);
  CHECK_FALSE(game::Grid::diff(original, restored));
  CHECK(restored.is_solid({-8, -8, -8}));
  CHECK(restored.is_solid({7, 7, 7}));
}

TEST_CASE("Grid diff distinguishes bounds and cell contents") {
  auto const base = make_grid();
  SECTION("identical grids do not differ") {
    CHECK_FALSE(game::Grid::diff(base, make_grid()));
  }
  SECTION("differing bounds differ") {
    auto const other = game::Grid{
      {.bounds = math::ibox3{math::ivec3{0, 0, 0}, math::ivec3{7, 7, 7}}}};
    CHECK(game::Grid::diff(base, other));
  }
  SECTION("differing cells differ") {
    auto other = make_grid();
    other.set_block({0, 0, 0}, game::Block::solid);
    CHECK(game::Grid::diff(base, other));
  }
}
