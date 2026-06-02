#include "game/grid.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
void set_solid(
  fpsparty::game::Grid &grid, Eigen::Vector3i const &cell_indices) {
  grid.fill(
    Eigen::AlignedBox3i{
      cell_indices,
      cell_indices + Eigen::Vector3i::Ones(),
    });
}
} // namespace

TEST_CASE("Grid raycast returns immediate hit for solid origin cell") {
  auto grid = fpsparty::game::Grid{{.width = 4, .height = 4, .depth = 4}};
  set_solid(grid, {1, 1, 1});
  auto const hit = grid.raycast(
    {1, 1, 1},
    Eigen::Vector3f::Constant(0.5f),
    Eigen::Vector3f::UnitX(),
    10.0f);
  REQUIRE(hit);
  CHECK(hit->cell_indices == Eigen::Vector3i{1, 1, 1});
  CHECK(hit->normal == Eigen::Vector3i::Zero());
  CHECK(hit->t == Catch::Approx(0.0f));
}

TEST_CASE("Grid raycast finds axis-aligned hits") {
  auto grid = fpsparty::game::Grid{{.width = 4, .height = 4, .depth = 4}};
  set_solid(grid, {2, 1, 1});
  set_solid(grid, {1, 0, 1});
  auto const x_hit = grid.raycast(
    {0, 1, 1},
    Eigen::Vector3f::Constant(0.5f),
    Eigen::Vector3f::UnitX(),
    10.0f);
  REQUIRE(x_hit);
  CHECK(x_hit->cell_indices == Eigen::Vector3i{2, 1, 1});
  CHECK(x_hit->normal == Eigen::Vector3i{-1, 0, 0});
  CHECK(x_hit->t == Catch::Approx(1.5f));
  auto const y_hit = grid.raycast(
    {1, 3, 1},
    {0.5f, 0.75f, 0.5f},
    Eigen::Vector3f{0.0f, -2.0f, 0.0f},
    10.0f);
  REQUIRE(y_hit);
  CHECK(y_hit->cell_indices == Eigen::Vector3i{1, 0, 1});
  CHECK(y_hit->normal == Eigen::Vector3i{0, 1, 0});
  CHECK(y_hit->t == Catch::Approx(1.375f));
}

TEST_CASE("Grid raycast steps all tied axes") {
  auto grid = fpsparty::game::Grid{{.width = 4, .height = 4, .depth = 4}};
  set_solid(grid, {1, 1, 0});
  auto const hit = grid.raycast(
    {0, 0, 0},
    Eigen::Vector3f::Constant(0.5f),
    {1.0f, 1.0f, 0.0f},
    10.0f);
  REQUIRE(hit);
  CHECK(hit->cell_indices == Eigen::Vector3i{1, 1, 0});
  CHECK(hit->normal == Eigen::Vector3i{-1, -1, 0});
  CHECK(hit->t == Catch::Approx(0.5f));
}

TEST_CASE("Grid raycast respects max t") {
  auto grid = fpsparty::game::Grid{{.width = 4, .height = 4, .depth = 4}};
  set_solid(grid, {2, 1, 1});
  auto const hit = grid.raycast(
    {0, 1, 1},
    Eigen::Vector3f::Constant(0.5f),
    Eigen::Vector3f::UnitX(),
    1.49f);
  CHECK_FALSE(hit);
}

TEST_CASE("Grid raycast misses empty cells") {
  auto grid = fpsparty::game::Grid{{.width = 4, .height = 4, .depth = 4}};
  auto const hit = grid.raycast(
    {0, 1, 1},
    Eigen::Vector3f::Constant(0.5f),
    Eigen::Vector3f::UnitX(),
    10.0f);
  CHECK_FALSE(hit);
}

TEST_CASE("Grid raycast treats out-of-bounds cells as non-solid") {
  auto grid = fpsparty::game::Grid{{.width = 4, .height = 4, .depth = 4}};
  set_solid(grid, {0, 1, 1});
  auto const hit = grid.raycast(
    {-2, 1, 1},
    Eigen::Vector3f::Constant(0.5f),
    Eigen::Vector3f::UnitX(),
    10.0f);
  REQUIRE(hit);
  CHECK(hit->cell_indices == Eigen::Vector3i{0, 1, 1});
  CHECK(hit->normal == Eigen::Vector3i{-1, 0, 0});
  CHECK(hit->t == Catch::Approx(1.5f));
}
