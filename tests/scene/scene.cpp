#include "scene/component_frame.hpp"
#include "scene/scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

namespace {
using namespace fpsparty;
using Catch::Approx;

constexpr auto keyframe_duration = 0.1f;

scene::Scene make_scene() {
  return scene::Scene{{.keyframe_duration = keyframe_duration}};
}

scene::elements::Camera camera_at(u64 key, float x) {
  return {.key = key, .position = {x, 0.0f, 0.0f}};
}

scene::elements::Box box_at(u64 key, float x) {
  return {
    .key = key,
    .half_extents = math::vec3::Ones(),
    .position = {x, 0.0f, 0.0f},
    .orientation = math::quat::Identity(),
  };
}

scene::Keyframe make_keyframe(
  std::uint64_t number,
  std::vector<scene::elements::Camera> cameras = {},
  std::vector<scene::elements::Box> boxes = {}) {
  return {
    .number = number,
    .grid = game::Grid{{}},
    .components = {
      .cameras = std::move(cameras),
      .distant_lights = {},
      .point_lights = {},
      .boxes = std::move(boxes),
      .humanoids = {},
      .items = {},
    },
  };
}
} // namespace

TEST_CASE("Component interpolation retains the base frame identity and order") {
  auto const a = scene::Component_frame{
    .cameras = {camera_at(1, 0.0f), camera_at(2, 4.0f)},
    .distant_lights = {},
    .point_lights = {},
    .boxes = {},
    .humanoids = {},
    .items = {{.entity_id = 4, .position = {2.0f, 0.0f, 0.0f}}},
  };
  auto const b = scene::Component_frame{
    .cameras = {camera_at(3, 9.0f), camera_at(1, 2.0f)},
    .distant_lights = {},
    .point_lights = {},
    .boxes = {},
    .humanoids = {},
    .items = {
      {.entity_id = 5, .position = {8.0f, 0.0f, 0.0f}},
      {.entity_id = 4, .position = {6.0f, 0.0f, 0.0f}},
    },
  };

  auto const result = scene::interpolate(a, b, scene::Component_frame_index{b}, 0.5f);

  REQUIRE(result.cameras.size() == 2);
  CHECK(result.cameras[0].key == 1);
  CHECK(result.cameras[0].position.x() == Approx(1.0f));
  CHECK(result.cameras[1].key == 2);
  CHECK(result.cameras[1].position.x() == Approx(4.0f));
  REQUIRE(result.items.size() == 1);
  CHECK(result.items[0].entity_id == 4);
  CHECK(result.items[0].position.x() == Approx(4.0f));
}

TEST_CASE("Component interpolation blends each render field") {
  auto const a = scene::Component_frame{
    .cameras = {},
    .distant_lights = {{
      .key = 1,
      .direction = math::vec3::UnitX(),
      .irradiance = math::vec3::Constant(2.0f),
    }},
    .point_lights = {{
      .key = 2,
      .position = math::vec3::Zero(),
      .irradiance = math::vec3::Zero(),
    }},
    .boxes = {box_at(3, 0.0f)},
    .humanoids = {{.entity_id = 4, .position = math::vec3::Zero()}},
    .items = {},
  };
  auto b = a;
  b.distant_lights[0].direction = math::vec3::UnitY();
  b.distant_lights[0].irradiance = math::vec3::Constant(4.0f);
  b.point_lights[0].position = math::vec3::Constant(2.0f);
  b.point_lights[0].irradiance = math::vec3::Constant(6.0f);
  b.boxes[0].half_extents = math::vec3::Constant(3.0f);
  b.boxes[0].orientation = math::quat{Eigen::AngleAxisf{
    std::numbers::pi_v<float>, math::vec3::UnitY()}};
  b.humanoids[0].position = math::vec3::Constant(4.0f);
  b.humanoids[0].pitch = 2.0f;
  b.humanoids[0].yaw = 4.0f;

  auto const result = scene::interpolate(a, b, scene::Component_frame_index{b}, 0.5f);

  CHECK(result.distant_lights[0].direction.norm() == Approx(1.0f));
  CHECK(result.distant_lights[0].irradiance.x() == Approx(3.0f));
  CHECK(result.point_lights[0].position.x() == Approx(1.0f));
  CHECK(result.point_lights[0].irradiance.x() == Approx(3.0f));
  CHECK(result.boxes[0].half_extents.x() == Approx(2.0f));
  CHECK(math::quat::Identity().angularDistance(result.boxes[0].orientation) ==
        Approx(std::numbers::pi_v<float> * 0.5f).margin(0.0001f));
  CHECK(result.humanoids[0].position.x() == Approx(2.0f));
  CHECK(result.humanoids[0].pitch == Approx(1.0f));
  CHECK(result.humanoids[0].yaw == Approx(2.0f));
}

TEST_CASE("Component rendering expands entities into stable box keys") {
  auto const components = scene::Component_frame{
    .cameras = {},
    .distant_lights = {},
    .point_lights = {},
    .boxes = {},
    .humanoids = {{.entity_id = 7, .position = math::vec3::Zero()}},
    .items = {{.entity_id = 8, .position = math::vec3::Ones()}},
  };

  auto const frame = components.render();

  REQUIRE(frame.boxes.size() == 3);
  CHECK(frame.boxes[0].key == scene::elements::entity_part_key(7, 0));
  CHECK(frame.boxes[1].key == scene::elements::entity_part_key(7, 1));
  CHECK(frame.boxes[2].key == scene::elements::entity_part_key(8, 0));
  CHECK(frame.boxes[2].position.x() == Approx(1.0f));
}

TEST_CASE("Scene playback consumes latency and trims old keyframes") {
  auto timeline = make_scene();
  for (auto number = std::uint64_t{10}; number <= 13; ++number) {
    timeline.push(make_keyframe(number));
  }
  REQUIRE(timeline.get_latency() == Approx(3 * keyframe_duration));

  CHECK(timeline.play(2.5f * keyframe_duration));

  CHECK(timeline.get_latency() == Approx(0.5f * keyframe_duration));
  CHECK(timeline.get_keyframe_count() == 2);
  CHECK(timeline.get_keyframe_number() == 12);
  CHECK(timeline.get_inter_keyframe_time() == Approx(0.5f));
}

TEST_CASE("Scene playback starves and freezes temporal history") {
  auto timeline = make_scene();
  timeline.push(make_keyframe(10, {}, {box_at(1, 0.0f)}));
  timeline.push(make_keyframe(11, {}, {box_at(1, 1.0f)}));
  timeline.play(0.5f * keyframe_duration);
  timeline.play(keyframe_duration);

  CHECK_FALSE(timeline.play(keyframe_duration));
  auto const current = timeline.get_box(1);
  auto const previous = timeline.get_previous_box(1);
  REQUIRE(current != nullptr);
  REQUIRE(previous != nullptr);
  CHECK(current->position.x() == Approx(0.5f));
  CHECK(previous->position.x() == Approx(0.5f));
}

TEST_CASE("Scene exposes current and previous frames with keyed lookup") {
  auto timeline = make_scene();
  timeline.push(make_keyframe(10, {camera_at(1, 0.0f)}, {box_at(2, 0.0f)}));
  timeline.push(make_keyframe(11, {camera_at(1, 2.0f)}, {box_at(2, 4.0f)}));

  timeline.play(0.5f * keyframe_duration);

  REQUIRE(timeline.get_current_frame().cameras.size() == 1);
  REQUIRE(timeline.get_previous_frame() != nullptr);
  REQUIRE(timeline.get_camera(1) != nullptr);
  REQUIRE(timeline.get_previous_camera(1) != nullptr);
  REQUIRE(timeline.get_box(2) != nullptr);
  REQUIRE(timeline.get_previous_box(2) != nullptr);
  CHECK(timeline.get_camera(1)->position.x() == Approx(1.0f));
  CHECK(timeline.get_previous_camera(1)->position.x() == Approx(0.0f));
  CHECK(timeline.get_box(2)->position.x() == Approx(2.0f));
  CHECK(timeline.get_previous_box(2)->position.x() == Approx(0.0f));
  CHECK(timeline.get_camera(9) == nullptr);
}

TEST_CASE("Scene latency adjustment clamps to buffered history") {
  auto timeline = make_scene();
  timeline.push(make_keyframe(10));
  timeline.push(make_keyframe(11));

  CHECK(timeline.set_latency(10 * keyframe_duration) ==
        Approx(keyframe_duration));
  CHECK(timeline.get_latency() == Approx(keyframe_duration));
}
