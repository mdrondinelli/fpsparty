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

scene::Keyframe make_keyframe(std::uint64_t number) {
  return scene::Keyframe{
    .number = number,
    .grid = game::Grid{{}},
    .cameras = {},
    .mesh_instances = {},
    .sun_direction = {},
  };
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
