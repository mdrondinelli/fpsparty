#include "game/entity_world.hpp"

#include <cstdint>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "game/humanoid.hpp"
#include "game/item.hpp"
#include "game/player.hpp"

namespace {
using fpsparty::game::Entity_handle;
using fpsparty::game::Entity_world;
using fpsparty::game::Humanoid;
using fpsparty::game::Player;
} // namespace

TEST_CASE("Entity world hash table supports lookup update and removal") {
  auto table = Entity_world::Hash_table{8};

  table.push(1, 10);
  table.push(9, 90);
  table.push(17, 170);

  REQUIRE(table.get(1));
  CHECK(*table.get(1) == 10);
  REQUIRE(table.get(9));
  CHECK(*table.get(9) == 90);
  REQUIRE(table.get(17));
  CHECK(*table.get(17) == 170);

  CHECK(table.exchange(9, 91) == 90);
  CHECK(*table.get(9) == 91);
  CHECK(table.pop(9) == 91);
  CHECK_FALSE(table.get(9));
  CHECK(*table.get(1) == 10);
  CHECK(*table.get(17) == 170);
}

TEST_CASE("Entity world hash table rehash preserves entries") {
  auto table = Entity_world::Hash_table{128};

  for (auto key = std::uint32_t{1}; key != 101; ++key) {
    table.push(key, key * 10);
  }

  CHECK(table.buckets().size() == 128);
  for (auto key = std::uint32_t{1}; key != 101; ++key) {
    REQUIRE(table.get(key));
    CHECK(*table.get(key) == key * 10);
  }

  table.rehash(256);
  CHECK(table.buckets().size() == 256);
  for (auto key = std::uint32_t{1}; key != 101; ++key) {
    REQUIRE(table.get(key));
    CHECK(*table.get(key) == key * 10);
  }
}

TEST_CASE("Entity world grows its entity index table") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto handles = std::vector<Entity_handle<Player>>{};

  for (auto i = 0; i != 100; ++i) {
    handles.push_back(world.emplace<Player>().handle);
  }

  for (auto const handle : handles) {
    CHECK(world.get(handle) != nullptr);
  }
}

TEST_CASE("Entity world exposes registered empty typed storage") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  world.register_entity_type<Humanoid>();

  CHECK(world.get_all<Player>().size() == 0);
  CHECK(world.get_all<Humanoid>().size() == 0);
  CHECK(world.get(Entity_handle<Player>{}) == nullptr);
}

TEST_CASE("Entity world assigns globally unique ids across entity types") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  world.register_entity_type<Humanoid>();

  auto const player = world.emplace<Player>().handle;
  auto const humanoid = world.emplace<Humanoid>().handle;

  CHECK(player.id == 1);
  CHECK(humanoid.id == 2);
  CHECK(player.id != humanoid.id);
  CHECK(world.get(player) != nullptr);
  CHECK(world.get(humanoid) != nullptr);
}

TEST_CASE("Entity world traversal exposes entities with typed handles") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace<Player>().handle;
  auto const second = world.emplace<Player>().handle;

  auto expected_id = std::uint32_t{1};
  for (auto [player, handle] : world.get_all<Player>()) {
    player.input_state.yaw = static_cast<float>(handle.id);
    CHECK(handle.id == expected_id++);
  }

  CHECK(world.get(first)->input_state.yaw == 1.0f);
  CHECK(world.get(second)->input_state.yaw == 2.0f);
}

TEST_CASE("Entity world range supports indexed access") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace<Player>().handle;
  auto const second = world.emplace<Player>().handle;
  auto const range = world.get_all<Player>();

  CHECK(range[0].handle == first);
  CHECK(range[1].handle == second);
  range[1].entity.input_state.yaw = 2.0f;
  CHECK(world.get(second)->input_state.yaw == 2.0f);
}

TEST_CASE("Entity world ranges survive entity vector reallocation") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace<Player>().handle;
  auto const range = world.get_all<Player>();
  auto const first_iterator = range.begin();

  for (auto i = 0; i != 100; ++i) {
    world.emplace<Player>();
  }

  CHECK(range.size() == 101);
  CHECK(range[0].handle == first);
  CHECK((*first_iterator).handle == first);
}

TEST_CASE("Entity world const ranges expose const entries") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  world.emplace<Player>();
  auto const &const_world = world;
  auto const range = const_world.get_all<Player>();

  static_assert(
    std::is_const_v<std::remove_reference_t<decltype(range[0].entity)>>);
  static_assert(
    std::is_same_v<decltype(range[0].handle), Entity_handle<Player const>>);
  CHECK(range.size() == 1);
}

TEST_CASE("Entity world range finds entities by handle") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace<Player>().handle;
  auto const second = world.emplace<Player>().handle;
  auto const range = world.get_all<Player>();

  CHECK((*range.find(first)).handle == first);
  CHECK((*range.find(second)).handle == second);
  CHECK(range.find(Entity_handle<Player>{}) == range.end());
  CHECK(range.find(Entity_handle<Player>{999}) == range.end());
}

TEST_CASE("Entity world const range finds mutable handles") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const handle = world.emplace<Player>().handle;
  auto const &const_world = world;
  auto const range = const_world.get_all<Player>();

  auto const it = range.find(handle);

  REQUIRE(it != range.end());
  CHECK((*it).handle.id == handle.id);
}

TEST_CASE("Entity world range erases a found entity") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace<Player>().handle;
  auto const second = world.emplace<Player>().handle;
  auto const range = world.get_all<Player>();

  auto const next = range.erase(range.find(first));

  CHECK(world.get(first) == nullptr);
  CHECK(world.get(second) != nullptr);
  REQUIRE(next != range.end());
  CHECK((*next).handle == second);
}

TEST_CASE("Entity world range erase continues at the moved entity") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace<Player>().handle;
  auto const middle = world.emplace<Player>().handle;
  auto const last = world.emplace<Player>().handle;
  auto range = world.get_all<Player>();
  auto position = range.begin();
  ++position;

  auto const next = range.erase(position);

  CHECK(world.get(middle) == nullptr);
  CHECK(world.get(first) != nullptr);
  CHECK(world.get(last) != nullptr);
  REQUIRE(next != range.end());
  CHECK((*next).handle == last);
}

TEST_CASE("Entity world range erase returns end after the last entity") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const handle = world.emplace<Player>().handle;
  auto range = world.get_all<Player>();

  auto const next = range.erase(range.begin());

  CHECK(next == range.end());
  CHECK(world.get(handle) == nullptr);
}

TEST_CASE("Entity world range can erase while iterating") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace<Player>().handle;
  auto const second = world.emplace<Player>().handle;
  auto const third = world.emplace<Player>().handle;
  auto range = world.get_all<Player>();

  for (auto it = range.begin(); it != range.end();) {
    it = range.erase(it);
  }

  CHECK(range.size() == 0);
  CHECK(world.get(first) == nullptr);
  CHECK(world.get(second) == nullptr);
  CHECK(world.get(third) == nullptr);
}

TEST_CASE("Entity world repairs lookup after swap-back erasure") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace<Player>().handle;
  auto const middle = world.emplace<Player>().handle;
  auto const last = world.emplace<Player>().handle;
  world.get(last)->input_state.pitch = 3.0f;

  world.remove(middle);

  CHECK(world.get(middle) == nullptr);
  CHECK(world.get(first) != nullptr);
  REQUIRE(world.get(last));
  CHECK(world.get(last)->input_state.pitch == 3.0f);
  CHECK(world.get_all<Player>().size() == 2);
}
