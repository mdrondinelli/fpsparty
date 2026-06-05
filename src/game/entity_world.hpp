#ifndef FPSPARTY_GAME_ENTITY_WORLD_HPP
#define FPSPARTY_GAME_ENTITY_WORLD_HPP

#include <vector>

#include <net/core/entity_id.hpp>
#include <serial/reader.hpp>
#include <serial/writer.hpp>

namespace fpsparty::game {

struct Entity_world_dump_info {
  serial::Writer *writer{};
};

struct Entity_world_load_info {
  std::span<serial::Reader *const> readers{};
};

template <typename... EntityTypes> class Entity_world {
public:
  std::size_t capacity() const noexcept {
    return std::get<0>(_entities).size();
  }

  void reserve(std::size_t new_capacity) {
    auto const curr_capacity = capacity();
    if (new_capacity > curr_capacity) {
      for_each_entity_vector([&](auto &entities) {
        entities.resize(new_capacity);
      });
      for (auto i = std::size_t{}; i != entity_type_count; ++i) {
        _generations[i].resize(new_capacity);
      }
      for (auto i = std::size_t{}; i != entity_type_count; ++i) {
        _free_indices[i].reserve(new_capacity);
        for (auto j = curr_capacity; j != new_capacity; ++j) {
          _free_indices[i].emplace_back(j);
        }
      }
    }
  }

  template <typename EntityType, typename... Args>
  EntityType &emplace_entity(Args &&...args) {
    auto &entities = get_entities_of_type<EntityType>();
    auto &free_indices = _free_indices[get_entity_type_index<EntityType>()];
    return *new (&storage.bytes) EntityType(std::forward<Args>(args)...);
  }

  void erase_entity(net::Entity_id id) {
    
  }

  template <typename EntityType>
  EntityType &get_entity(net::Entity_id id) noexcept {
    return get_entities_of_type<EntityType>()[id];
  }

  template <typename EntityType>
  EntityType const &get_entity(net::Entity_id id) const noexcept {
    return get_entities_of_type<EntityType>()[id];
  }

  template <typename EntityType>
  constexpr std::vector<EntityType> &get_entities_of_type() noexcept {
    return std::get<std::vector<EntityType>>(_entities);
  }

  template <typename EntityType>
  constexpr std::vector<EntityType> const &
  get_entities_of_type() const noexcept {
    return std::get<std::vector<EntityType>>(_entities);
  }

private:
  static constexpr std::size_t entity_type_count = sizeof...(EntityTypes);
  static constexpr std::index_sequence_for<EntityTypes...> entity_type_indices{};

  template <typename EntityType>
  static constexpr std::size_t get_entity_type_index() noexcept {
    return get_entity_type_index_impl<EntityType, EntityTypes...>();
  }

  template <typename EntityType, typename EntityTypesHead, typename... EntityTypesTail>
  static constexpr std::size_t get_entity_type_index_impl() noexcept {
    if constexpr (std::is_same_v<EntityType, EntityTypesHead>) {
      return 0;
    } else {
      return 1 + get_entity_type_index_impl<EntityType, EntityTypesTail...>();
    }
  }

  template <typename F, std::size_t... I>
  void for_each_entity_vector(
    F f, std::index_sequence<I...> = std::index_sequence_for<EntityTypes...>{}) const noexcept {
    (f(std::get<I>(_entities)), ...);
  }

  std::tuple<std::vector<EntityType>>...> _entities;
  std::array<std::vector<std::uint32_t>, entity_type_count> _generations{};
  std::array<std::vector<std::uint32_t>, entity_type_count> _free_indices{};
};

// New idea:
// Each entity type is densely packed.
// No holes.
// Swap back entity into would-be hold on delete.
// Add new entity to end of vector.
// Entity id is unique 32 bit value. (can just be incremented)
// Hash map entity id to slot in vector.
// Fixup when entity is deleted.
//
// Problem:
// If I want to iterate over all entities of a given type,
// I won't know the ids of the entities as I iterate.
// I could add a reverse lookup map.
// One extra hash-map insert/erase per entity add/remove.
// Acceptable. Integer hash map is fast.

} // namespace fpsparty::game

#endif
