// Tests for the latency-based playback clock in scene::Scene.
//
// API contract under test (implemented separately):
//   struct Scene_create_info { float keyframe_duration; };
//   float Scene::get_latency() const noexcept;       // (newest - P) * keyframe_duration, 0 if empty
//   bool  Scene::play(float dt);                    // false when starved; clamps P at newest
//   void  Scene::set_latency(float seconds) noexcept;
//   std::size_t Scene::get_keyframe_count() const noexcept;
//
// All pure: keyframes are pushed directly as plain structs, no serialization,
// no sockets.

#include "scene/scene.hpp"

#include "game/grid.hpp"
#include "scene/keyframe.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace {
using namespace fpsparty;
using Catch::Approx;

constexpr float kd = 0.1f;

scene::Scene make_scene() { return scene::Scene{{.keyframe_duration = kd}}; }

scene::Keyframe make_keyframe(
  std::uint64_t number,
  std::vector<scene::Identified<scene::Camera>> cameras = {},
  std::vector<scene::Identified<scene::Mesh_instance>> mesh_instances = {}) {
  return scene::Keyframe{
    .number = number,
    .grid = game::Grid{{}},
    .cameras = std::move(cameras),
    .mesh_instances = std::move(mesh_instances),
    .sun_direction = {},
  };
}

scene::Identified<scene::Camera> camera_at(std::uint64_t id, float x) {
  return {id, scene::Camera{.position = {x, 0.0f, 0.0f}}};
}

scene::Identified<scene::Mesh_instance> instance_at(std::uint64_t id, float x) {
  return {
    id,
    scene::Mesh_instance{
      .mesh = scene::Mesh::cube,
      .position = {x, 0.0f, 0.0f},
    }};
}
} // namespace

TEST_CASE("Scene latency is the keyframe lead in seconds") {
  auto scene = make_scene();
  scene.push(make_keyframe(10));
  CHECK(scene.get_latency() == Approx(0.0f)); // playback pinned to first keyframe
  scene.push(make_keyframe(11));
  CHECK(scene.get_latency() == Approx(kd)); // one tick ahead
  scene.push(make_keyframe(13));
  CHECK(scene.get_latency() == Approx(3 * kd)); // three ticks ahead
}

TEST_CASE("Scene play advances playback and consumes latency") {
  auto scene = make_scene();
  scene.push(make_keyframe(10));
  scene.push(make_keyframe(12)); // lead = 2 ticks
  CHECK(scene.play(kd));                 // advance one tick, forward frame remains
  CHECK(scene.get_latency() == Approx(kd));
}

TEST_CASE("Scene play starves and clamps at the newest keyframe") {
  auto scene = make_scene();
  scene.push(make_keyframe(10));
  scene.push(make_keyframe(11)); // lead = 1 tick
  CHECK(scene.play(kd)); // advances onto newest (and trims) -> progress
  CHECK(scene.get_latency() == Approx(0.0f));
  CHECK_FALSE(scene.play(kd)); // stuck at newest, nothing to advance -> starved
  CHECK(scene.get_latency() == Approx(0.0f));
}

TEST_CASE("Scene set_latency snaps the lead to the target") {
  auto scene = make_scene();
  for (auto n = std::uint64_t{10}; n <= 20; ++n) {
    scene.push(make_keyframe(n));
  }
  REQUIRE(scene.get_latency() == Approx(10 * kd)); // lead = 10 ticks
  scene.set_latency(2 * kd);
  CHECK(scene.get_latency() == Approx(2 * kd));
}

TEST_CASE("Scene set_latency clamps to the available buffer") {
  auto scene = make_scene();
  scene.push(make_keyframe(10));
  scene.push(make_keyframe(11)); // only 1 tick of buffer
  scene.set_latency(10 * kd);         // ask for more lead than exists
  CHECK(scene.get_latency() == Approx(kd)); // clamped to the oldest keyframe
}

TEST_CASE("Scene play trims keyframes behind the playback point") {
  auto scene = make_scene();
  for (auto n = std::uint64_t{10}; n <= 13; ++n) {
    scene.push(make_keyframe(n));
  }
  REQUIRE(scene.get_keyframe_count() == 4);
  scene.play(2.5f * kd); // playback -> 12, keeping 12 as the interpolation base
  CHECK(scene.get_keyframe_count() == 2); // 12 (base) and 13
}

TEST_CASE("Scene interpolated lookup by id") {
  auto scene = make_scene();
  scene.push(make_keyframe(10, {camera_at(1, 0.0f)}, {instance_at(1, 0.0f)}));
  scene.push(make_keyframe(11, {camera_at(1, 1.0f)}, {instance_at(1, 1.0f)}));
  scene.play(0.5f * kd); // t = 0.5 between keyframes 10 and 11
  auto const camera = scene.get_interpolated_camera(1);
  REQUIRE(camera != nullptr);
  CHECK(camera->position.x() == Approx(0.5f));
  auto const instance = scene.get_interpolated_mesh_instance(1);
  REQUIRE(instance != nullptr);
  CHECK(instance->position.x() == Approx(0.5f));
  CHECK(scene.get_interpolated_camera(2) == nullptr);
  CHECK(scene.get_interpolated_mesh_instance(2) == nullptr);
}

TEST_CASE("Scene previous interpolation lags current by one play") {
  auto scene = make_scene();
  scene.push(make_keyframe(10, {}, {instance_at(1, 0.0f)}));
  scene.push(make_keyframe(11, {}, {instance_at(1, 1.0f)}));
  scene.push(make_keyframe(12, {}, {instance_at(1, 2.0f)}));
  scene.play(0.5f * kd); // current: t = 0.5; no previous yet
  CHECK(scene.get_previous_interpolated_mesh_instance(1) == nullptr);
  scene.play(0.5f * kd); // current: exactly keyframe 11
  auto const curr = scene.get_interpolated_mesh_instance(1);
  auto const prev = scene.get_previous_interpolated_mesh_instance(1);
  REQUIRE(curr != nullptr);
  REQUIRE(prev != nullptr);
  CHECK(curr->position.x() == Approx(1.0f));
  CHECK(prev->position.x() == Approx(0.5f));
}

TEST_CASE("Scene keeps the last interpolation when starved") {
  auto scene = make_scene();
  scene.push(make_keyframe(10, {}, {instance_at(1, 0.0f)}));
  scene.push(make_keyframe(11, {}, {instance_at(1, 1.0f)}));
  scene.play(0.5f * kd); // t = 0.5
  scene.play(kd);        // clamps at keyframe 11 and starves
  auto const curr = scene.get_interpolated_mesh_instance(1);
  auto const prev = scene.get_previous_interpolated_mesh_instance(1);
  REQUIRE(curr != nullptr);
  REQUIRE(prev != nullptr);
  CHECK(curr->position.x() == Approx(0.5f)); // frozen at the last sample
  CHECK(prev->position.x() == Approx(0.5f)); // previous == current: no motion
}

TEST_CASE("Scene previous lookup outlives trimmed keyframes") {
  auto scene = make_scene();
  scene.push(make_keyframe(10, {camera_at(7, 3.0f)}, {instance_at(2, 5.0f)}));
  scene.push(make_keyframe(11));
  scene.push(make_keyframe(12));
  scene.play(0.5f * kd); // interpolation carries ids forward from keyframe 10
  scene.play(0.5f * kd); // keyframe 10 trimmed; previous still references it
  CHECK(scene.get_interpolated_camera(7) == nullptr);
  CHECK(scene.get_interpolated_mesh_instance(2) == nullptr);
  auto const prev_camera = scene.get_previous_interpolated_camera(7);
  REQUIRE(prev_camera != nullptr);
  CHECK(prev_camera->position.x() == Approx(3.0f));
  auto const prev_instance = scene.get_previous_interpolated_mesh_instance(2);
  REQUIRE(prev_instance != nullptr);
  CHECK(prev_instance->position.x() == Approx(5.0f));
}
