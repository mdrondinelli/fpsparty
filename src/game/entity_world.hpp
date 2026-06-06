#ifndef FPSPARTY_GAME_ENTITY_WORLD_HPP
#define FPSPARTY_GAME_ENTITY_WORLD_HPP

#include <bit>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <net/core/entity_id.hpp>
#include <serial/reader.hpp>
#include <serial/writer.hpp>

#include "entity_handle.hpp"
#include "entity_traits.hpp"

namespace fpsparty::game {

struct Entity_world_dump_info {
  serial::Writer *writer{};
};

struct Entity_world_load_info {
  std::span<serial::Reader *const> readers{};
};

// EntityTypes must be able to be stored in a std::vector.
class Entity_world {
public:
  class Hash_table {
  public:
    struct Entry {
      std::uint32_t key{};
      std::uint32_t value{};

      constexpr Entry() noexcept = default;

      constexpr Entry(std::uint32_t key, std::uint32_t value)
          : key{key}, value{value} {
        // if you're trying to construct the empty key, use the default ctor
        assert(key != 0);
      }

      constexpr bool empty() const noexcept { return key == 0; }

      constexpr void reset() noexcept { key = 0; }
    };

    constexpr Hash_table() noexcept = default;

    explicit Hash_table(std::uint32_t bucket_count)
        : _buckets(std::make_unique<Entry[]>(bucket_count)) {
      // contract: new_bucket_count must be a power of 2
      assert(std::popcount(bucket_count) == 1);
    }

    void rehash(std::uint32_t new_bucket_count) {
      // contract: new_bucket_count must be a power of 2
      assert(std::popcount(new_bucket_count) == 1);
      auto temp = Hash_table{new_bucket_count};
      auto live_entry_count = 0u;
      auto const buckets = std::span{_buckets.get(), _bucket_count};
      for (auto const &bucket : buckets) {
        if (!bucket.empty()) {
          ++live_entry_count;
          // contract: new_bucket_count must exceed live entry count
          assert(new_bucket_count > live_entry_count);
          temp.push(bucket);
        }
      }
    }

    std::optional<std::uint32_t> get(std::uint32_t key) const noexcept {
      assert(key != 0);
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      for (;;) {
        auto const &entry = _buckets[i];
        if (entry.empty()) {
          return std::nullopt;
        } else if (entry.key == key) {
          return entry.value;
        } else {
          i = (i + 1) & mask;
        }
      }
    }

    void push(Entry entry) noexcept {
      assert(!entry.empty());
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(entry.key, bits);
      auto d = 0u;
      for (;;) {
        // contract: must not insert into a full table
        assert(d < _bucket_count);
        auto &bucket = _buckets[i];
        if (bucket.empty()) {
          _buckets[i] = entry;
          return;
        } else {
          // contract: double inserts not allowed
          assert(bucket.key != entry.key);
          auto const bucket_hash = hash(bucket.key, bits);
          auto const bucket_d = (i - bucket_hash) & mask;
          if (bucket_d < d) {
            std::swap(bucket, entry);
          }
          ++i;
          ++d;
        }
      }
    }

    std::uint32_t pop(std::uint32_t key) {
      assert(key != 0);
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      for (;;) {
        auto &bucket = _buckets[i];
        // contract: key must be in table
        assert(!bucket.empty());
        if (bucket.key == key) {
          auto const result = bucket.value;
          for (;;) {
            auto const j = (i + 1) & mask;
            auto &bucket_j = _buckets[j];
            if (bucket_j.empty() || hash(bucket_j.key, bits) == j) {
              _buckets[i].reset();
              return result;
            }
            _buckets[i] = _buckets[j];
            i = j;
          }
        } else {
          i = (i + 1) & mask;
        }
      }
    }

    std::uint32_t exchange(std::uint32_t key, std::uint32_t value) noexcept {
      assert(key != 0);
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      for (;;) {
        auto &bucket = _buckets[i];
        // contract: key must be in table
        assert(!bucket.empty());
        if (bucket.key == key) {
          auto const result = bucket.value;
          bucket.value = value;
          return result;
        } else {
          i = (i + 1) & mask;
        }
      }
    }

    std::span<Entry const> buckets() const noexcept {
      return std::span{_buckets.get(), _bucket_count};
    }

  private:
    static constexpr std::uint32_t hash(std::uint32_t key, int bits) noexcept {
      return (key * 2654435769u) >> (32 - bits);
    }

    std::unique_ptr<Entry[]> _buckets{};
    std::uint32_t _bucket_count{};
  };

  template <typename EntityType, typename... Args>
  std::pair<std::reference_wrapper<EntityType>, Entity_handle<EntityType>> &
  emplace_entity(Args &&...args) {
    auto &entity_array = get_entity_array_impl<EntityType>();
    auto &entity = entity_array.entities.emplace_back(std::forward<Args>(args)...);
    auto const id = entity_array.entity_ids.emplace_back(entity_array.next_entity_id++);
    entity_array.index_lookup_table.push(
      entity_array.entity_ids.back(),
      static_cast<std::uint32_t>(entity_array.entities.size() - 1));
    return {std::ref(entity), Entity_handle<EntityType>{id}};
  }

  // Invokes undefined behavior if handle does not correspond to a non-erased
  // entity handle returned from emplace_entity. Unless handle is 0, then no
  // operation is performed.
  template <typename EntityType>
  void erase_entity(Entity_handle<EntityType> handle) {
    if (!handle) {
      return;
    }
    auto &entity_array = get_entity_array_impl<EntityType>();
    auto const index = entity_array.index_lookup_table.pop(handle.id);
    if (index + 1 < entity_array.entities.size()) {
      entity_array.entities[index] = std::move(entity_array.entities.back());
      entity_array.entities.pop_back();
      auto const moved_entity_id = entity_array.entity_ids.back();
      entity_array.entity_ids[index] = moved_entity_id;
      entity_array.entity_ids.pop_back();
      entity_array.index_lookup_table.update(moved_entity_id, index);
    } else {
      entity_array.entities.pop_back();
      entity_array.entity_ids.pop_back();
    }
  }

  // Returns a valid pointer iff handle corresponds to a non-erased entity
  // handle returned from emplace_entity. Otherwise returns nullptr.
  template <typename EntityType>
  EntityType const *
  get_entity(Entity_handle<EntityType> handle) const noexcept {
    auto &entity_array = get_entity_array_impl<EntityType>();
    auto const index =
      entity_array.index_lookup_table.try_lookup_index(handle.id);
    if (index) {
      return &entity_array.entities[*index];
    } else {
      return nullptr;
    }
  }

  template <typename EntityType>
  EntityType *get_entity(Entity_handle<EntityType> handle) noexcept {
    return const_cast<EntityType *>(const_cast<Entity_world const *>(this)
                                      ->get_entity<EntityType>(handle));
  }

  template <typename EntityType>
  constexpr std::span<EntityType> const &
  get_entities_of_type() const noexcept {
    return get_entity_array_impl<EntityType>().entities;
  }

  template <typename EntityType>
  constexpr std::span<EntityType> &get_entities_of_type() noexcept {
    return get_entity_array_impl<EntityType>().entities;
  }

  template <typename EntityType> void register_entity_type() {
    auto constexpr type_index =
      static_cast<std::size_t>(Entity_traits<EntityType>::type);
    while (_entity_arrays.size() <= type_index) {
      _entity_arrays.resize(_entity_arrays.size() * 2);
    }
    _entity_arrays[type_index] =
      std::make_unique<Entity_array_impl<EntityType>>();
  }

private:
  class Entity_array {
  public:
    virtual ~Entity_array() = default;
  };

  template <typename EntityType> class Entity_array_impl : public Entity_array {
  public:
    std::vector<EntityType> entities;
    std::vector<net::Entity_id> entity_ids;
    Hash_table index_lookup_table{512};
    net::Entity_id next_entity_id{1};
  };

  template <typename EntityType>
  Entity_array_impl<EntityType> &get_entity_array_impl() const noexcept {
    auto constexpr type_index =
      static_cast<std::size_t>(Entity_traits<EntityType>::type);
    return *static_cast<Entity_array_impl<EntityType> *>(
      _entity_arrays[type_index].get());
  }

  std::vector<std::unique_ptr<Entity_array>> _entity_arrays;
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
