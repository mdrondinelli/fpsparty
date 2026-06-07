#include "scene/scene.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Scene grid remesh flag is explicit") {
  auto scene = fpsparty::scene::Scene{};

  CHECK_FALSE(scene.get_grid_remesh_flag());

  scene.set_grid_remesh_flag();
  CHECK(scene.get_grid_remesh_flag());

  scene.clear_grid_remesh_flag();
  CHECK_FALSE(scene.get_grid_remesh_flag());
}
