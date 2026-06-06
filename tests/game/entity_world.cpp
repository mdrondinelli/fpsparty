#include "game/entity_world.hpp"
#include "game/game.hpp"
#include "game/humanoid.hpp"
#include "game/player.hpp"
#include "game/projectile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using fpsparty::game::Entity_handle;
using fpsparty::game::Entity_world;
using fpsparty::game::Game;
using fpsparty::game::Humanoid;
using fpsparty::game::Player;
using fpsparty::game::Projectile;
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
    handles.push_back(world.emplace_entity<Player>().handle);
  }

  for (auto const handle : handles) {
    CHECK(world.get_entity(handle) != nullptr);
  }
}

TEST_CASE("Entity world exposes registered empty typed storage") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  world.register_entity_type<Humanoid>();

  CHECK(world.get_entities<Player>().empty());
  CHECK(world.get_entities_with_handles<Humanoid>().size() == 0);
  CHECK(world.get_entity(Entity_handle<Player>{}) == nullptr);
}

TEST_CASE("Entity world assigns ids independently per entity type") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  world.register_entity_type<Humanoid>();

  auto const player = world.emplace_entity<Player>().handle;
  auto const humanoid = world.emplace_entity<Humanoid>().handle;

  CHECK(player.id == 1);
  CHECK(humanoid.id == 1);
  CHECK(world.get_entity(player) != nullptr);
  CHECK(world.get_entity(humanoid) != nullptr);
}

TEST_CASE("Entity world traversal exposes entities with typed handles") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace_entity<Player>().handle;
  auto const second = world.emplace_entity<Player>().handle;

  auto expected_id = std::uint32_t{1};
  for (auto [player, handle] :
       world.get_entities_with_handles<Player>()) {
    player.input_state.yaw = static_cast<float>(handle.id);
    CHECK(handle.id == expected_id++);
  }

  CHECK(world.get_entity(first)->input_state.yaw == 1.0f);
  CHECK(world.get_entity(second)->input_state.yaw == 2.0f);
}

TEST_CASE("Entity world range supports indexed access") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace_entity<Player>().handle;
  auto const second = world.emplace_entity<Player>().handle;
  auto const range = world.get_entities_with_handles<Player>();

  CHECK(range[0].handle == first);
  CHECK(range[1].handle == second);
  range[1].entity.input_state.yaw = 2.0f;
  CHECK(world.get_entity(second)->input_state.yaw == 2.0f);
}

TEST_CASE("Entity world ranges survive entity vector reallocation") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace_entity<Player>().handle;
  auto const range = world.get_entities_with_handles<Player>();
  auto const first_iterator = range.begin();

  for (auto i = 0; i != 100; ++i) {
    world.emplace_entity<Player>();
  }

  CHECK(range.size() == 101);
  CHECK(range[0].handle == first);
  CHECK((*first_iterator).handle == first);
}

TEST_CASE("Entity world const ranges expose const entries") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  world.emplace_entity<Player>();
  auto const &const_world = world;
  auto const range = const_world.get_entities_with_handles<Player>();

  static_assert(std::is_const_v<
                std::remove_reference_t<decltype(range[0].entity)>>);
  static_assert(std::is_same_v<
                decltype(range[0].handle), Entity_handle<Player const>>);
  CHECK(range.size() == 1);
}

TEST_CASE("Entity world repairs lookup after swap-back erasure") {
  auto world = Entity_world{};
  world.register_entity_type<Player>();
  auto const first = world.emplace_entity<Player>().handle;
  auto const middle = world.emplace_entity<Player>().handle;
  auto const last = world.emplace_entity<Player>().handle;
  world.get_entity(last)->input_state.pitch = 3.0f;

  world.erase_entity(middle);

  CHECK(world.get_entity(middle) == nullptr);
  CHECK(world.get_entity(first) != nullptr);
  REQUIRE(world.get_entity(last));
  CHECK(world.get_entity(last)->input_state.pitch == 3.0f);
  CHECK(world.get_entities<Player>().size() == 2);
}

TEST_CASE("Game replaces a stale player humanoid handle") {
  auto game = Game{{}};
  auto const player_handle = game.create_player();
  auto const old_humanoid_handle = game.create_humanoid();
  game.get_entities().get_entity(player_handle)->humanoid =
    old_humanoid_handle;
  game.get_entities().erase_entity(old_humanoid_handle);

  game.tick(0.01f);

  auto const *player = game.get_entities().get_entity(player_handle);
  REQUIRE(player);
  CHECK(player->humanoid);
  CHECK(player->humanoid != old_humanoid_handle);
  CHECK(game.get_entities().get_entity(player->humanoid) != nullptr);
}

TEST_CASE("Game clears a stale projectile creator handle") {
  auto game = Game{{}};
  auto const humanoid_handle = game.create_humanoid();
  auto const projectile_handle = game.create_projectile({
    .creator = humanoid_handle,
    .position = {0.0f, 10.0f, 0.0f},
  });
  game.get_entities().erase_entity(humanoid_handle);

  game.tick(0.01f);

  auto const *projectile =
    game.get_entities().get_entity(projectile_handle);
  REQUIRE(projectile);
  CHECK_FALSE(projectile->creator);
}

TEST_CASE("Game defers dense removals until traversal completes") {
  auto game = Game{{}};
  game.create_projectile({.position = {0.0f, -10.0f, 0.0f}});
  game.create_projectile({.position = {1.0f, -10.0f, 0.0f}});
  game.create_projectile({.position = {2.0f, -10.0f, 0.0f}});

  game.tick(0.01f);

  CHECK(game.get_entities().get_entities<Projectile>().empty());
}
