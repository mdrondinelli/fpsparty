#ifndef FPSPARTY_GAME_ENTITY_WORLD_HPP
#define FPSPARTY_GAME_ENTITY_WORLD_HPP

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <net/core/entity_id.hpp>

#include "entity_handle.hpp"
#include "entity_traits.hpp"

namespace fpsparty::game {

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
        assert(key != 0);
      }

      constexpr bool empty() const noexcept { return key == 0; }

      constexpr void reset() noexcept { key = 0; }
    };

    constexpr Hash_table() noexcept = default;

    explicit Hash_table(std::uint32_t bucket_count)
        : _buckets(std::make_unique<Entry[]>(bucket_count)),
          _bucket_count{bucket_count} {
      assert(bucket_count >= 2);
      assert(std::has_single_bit(bucket_count));
    }

    void rehash(std::uint32_t new_bucket_count) {
      assert(new_bucket_count >= 2);
      assert(std::has_single_bit(new_bucket_count));
      assert(new_bucket_count > _size);

      auto replacement = Hash_table{new_bucket_count};
      for (auto const &bucket : buckets()) {
        if (!bucket.empty()) {
          replacement.push_without_growing(bucket);
        }
      }
      swap(replacement);
    }

    std::optional<std::uint32_t> get(std::uint32_t key) const noexcept {
      assert(key != 0);
      if (_bucket_count == 0) {
        return std::nullopt;
      }

      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      for (;;) {
        auto const &entry = _buckets[i];
        if (entry.empty()) {
          return std::nullopt;
        }
        if (entry.key == key) {
          return entry.value;
        }
        i = (i + 1) & mask;
      }
    }

    void push(Entry entry) {
      assert(!entry.empty());
      ensure_insert_capacity();
      push_without_growing(entry);
    }

    void push(std::uint32_t key, std::uint32_t value) {
      push(Entry{key, value});
    }

    std::uint32_t pop(std::uint32_t key) noexcept {
      assert(key != 0);
      assert(_bucket_count != 0);

      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      for (;;) {
        auto &bucket = _buckets[i];
        assert(!bucket.empty());
        if (bucket.key == key) {
          auto const result = bucket.value;
          for (;;) {
            auto const j = (i + 1) & mask;
            auto &next_bucket = _buckets[j];
            if (next_bucket.empty() || hash(next_bucket.key, bits) == j) {
              _buckets[i].reset();
              --_size;
              return result;
            }
            _buckets[i] = next_bucket;
            i = j;
          }
        }
        i = (i + 1) & mask;
      }
    }

    std::uint32_t
    exchange(std::uint32_t key, std::uint32_t value) noexcept {
      assert(key != 0);
      assert(_bucket_count != 0);

      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      for (;;) {
        auto &bucket = _buckets[i];
        assert(!bucket.empty());
        if (bucket.key == key) {
          auto const result = bucket.value;
          bucket.value = value;
          return result;
        }
        i = (i + 1) & mask;
      }
    }

    std::span<Entry const> buckets() const noexcept {
      return {_buckets.get(), _bucket_count};
    }

    std::size_t size() const noexcept { return _size; }

  private:
    static constexpr std::uint32_t hash(std::uint32_t key, int bits) noexcept {
      return (key * 2654435769u) >> (32 - bits);
    }

    void ensure_insert_capacity() {
      if (_bucket_count == 0) {
        rehash(8);
      } else if (
        (static_cast<std::uint64_t>(_size) + 1) * 10 >=
        static_cast<std::uint64_t>(_bucket_count) * 7) {
        assert(_bucket_count <= (std::uint32_t{1} << 30));
        rehash(_bucket_count * 2);
      }
    }

    void push_without_growing(Entry entry) noexcept {
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(entry.key, bits);
      auto distance = std::uint32_t{};
      for (;;) {
        auto &bucket = _buckets[i];
        if (bucket.empty()) {
          bucket = entry;
          ++_size;
          return;
        }

        assert(bucket.key != entry.key);
        auto const bucket_hash = hash(bucket.key, bits);
        auto const bucket_distance = (i - bucket_hash) & mask;
        if (bucket_distance < distance) {
          std::swap(bucket, entry);
          distance = bucket_distance;
        }
        i = (i + 1) & mask;
        ++distance;
        assert(distance < _bucket_count);
      }
    }

    void swap(Hash_table &other) noexcept {
      _buckets.swap(other._buckets);
      std::swap(_bucket_count, other._bucket_count);
      std::swap(_size, other._size);
    }

    std::unique_ptr<Entry[]> _buckets{};
    std::uint32_t _bucket_count{};
    std::uint32_t _size{};
  };

  template <typename EntityType> struct Entity_entry {
    EntityType &entity;
    Entity_handle<std::remove_const_t<EntityType>> handle;
  };

  template <typename EntityType> class Entity_range {
  public:
    class Iterator {
    public:
      using difference_type = std::ptrdiff_t;
      using value_type = Entity_entry<EntityType>;
      using iterator_category = std::random_access_iterator_tag;

      constexpr value_type operator*() const noexcept {
        return {
          .entity = _entities[_index],
          .handle = {_entity_ids[_index]},
        };
      }

      constexpr Iterator &operator++() noexcept {
        ++_index;
        return *this;
      }

      constexpr Iterator operator++(int) noexcept {
        auto result = *this;
        ++*this;
        return result;
      }

      friend constexpr bool
      operator==(Iterator const &lhs, Iterator const &rhs) noexcept {
        return lhs._index == rhs._index;
      }

    private:
      friend class Entity_range;

      constexpr Iterator(
        std::span<EntityType> entities,
        std::span<net::Entity_id const> entity_ids,
        std::size_t index) noexcept
          : _entities{entities}, _entity_ids{entity_ids}, _index{index} {}

      std::span<EntityType> _entities;
      std::span<net::Entity_id const> _entity_ids;
      std::size_t _index{};
    };

    constexpr Iterator begin() const noexcept {
      return {_entities, _entity_ids, 0};
    }

    constexpr Iterator end() const noexcept {
      return {_entities, _entity_ids, _entities.size()};
    }

    constexpr std::size_t size() const noexcept { return _entities.size(); }

  private:
    friend class Entity_world;

    constexpr Entity_range(
      std::span<EntityType> entities,
      std::span<net::Entity_id const> entity_ids) noexcept
        : _entities{entities}, _entity_ids{entity_ids} {
      assert(_entities.size() == _entity_ids.size());
    }

    std::span<EntityType> _entities;
    std::span<net::Entity_id const> _entity_ids;
  };

  template <typename EntityType, typename... Args>
  std::pair<std::reference_wrapper<EntityType>, Entity_handle<EntityType>>
  emplace_entity(Args &&...args) {
    auto &entity_array = ensure_entity_array<EntityType>();
    auto &entity =
      entity_array.entities.emplace_back(std::forward<Args>(args)...);
    assert(entity_array.next_entity_id != 0);
    auto const id = entity_array.next_entity_id++;
    entity_array.entity_ids.push_back(id);
    entity_array.index_lookup_table.push(
      id, static_cast<std::uint32_t>(entity_array.entities.size() - 1));
    return {std::ref(entity), Entity_handle<EntityType>{id}};
  }

  template <typename EntityType>
  void erase_entity(Entity_handle<EntityType> handle) noexcept {
    if (!handle) {
      return;
    }

    auto &entity_array = ensure_entity_array<EntityType>();
    auto const index = entity_array.index_lookup_table.pop(handle.id);
    auto const last_index = entity_array.entities.size() - 1;
    if (index != last_index) {
      entity_array.entities[index] = std::move(entity_array.entities.back());
      auto const moved_entity_id = entity_array.entity_ids.back();
      entity_array.entity_ids[index] = moved_entity_id;
      entity_array.index_lookup_table.exchange(moved_entity_id, index);
    }
    entity_array.entities.pop_back();
    entity_array.entity_ids.pop_back();
  }

  template <typename EntityType>
  EntityType const *
  get_entity(Entity_handle<EntityType> handle) const noexcept {
    if (!handle) {
      return nullptr;
    }

    auto const *entity_array = find_entity_array<EntityType>();
    if (!entity_array) {
      return nullptr;
    }
    auto const index = entity_array->index_lookup_table.get(handle.id);
    return index ? &entity_array->entities[*index] : nullptr;
  }

  template <typename EntityType>
  EntityType *get_entity(Entity_handle<EntityType> handle) noexcept {
    return const_cast<EntityType *>(
      std::as_const(*this).get_entity<EntityType>(handle));
  }

  template <typename EntityType>
  std::span<EntityType const> get_entities_of_type() const noexcept {
    auto const *entity_array = find_entity_array<EntityType>();
    return entity_array ? std::span<EntityType const>{entity_array->entities}
                        : std::span<EntityType const>{};
  }

  template <typename EntityType>
  std::span<EntityType> get_entities_of_type() noexcept {
    return ensure_entity_array<EntityType>().entities;
  }

  template <typename EntityType>
  Entity_range<EntityType const> get_entities_with_handles() const noexcept {
    auto const *entity_array = find_entity_array<EntityType>();
    if (!entity_array) {
      return {{}, {}};
    }
    return {entity_array->entities, entity_array->entity_ids};
  }

  template <typename EntityType>
  Entity_range<EntityType> get_entities_with_handles() noexcept {
    auto &entity_array = ensure_entity_array<EntityType>();
    return {entity_array.entities, entity_array.entity_ids};
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
    Hash_table index_lookup_table{8};
    net::Entity_id next_entity_id{1};
  };

  template <typename EntityType>
  Entity_array_impl<EntityType> *find_entity_array() noexcept {
    auto const type_index =
      static_cast<std::size_t>(Entity_traits<EntityType>::type);
    if (type_index >= _entity_arrays.size() || !_entity_arrays[type_index]) {
      return nullptr;
    }
    return static_cast<Entity_array_impl<EntityType> *>(
      _entity_arrays[type_index].get());
  }

  template <typename EntityType>
  Entity_array_impl<EntityType> const *find_entity_array() const noexcept {
    return const_cast<Entity_world *>(this)->find_entity_array<EntityType>();
  }

  template <typename EntityType>
  Entity_array_impl<EntityType> &ensure_entity_array() {
    if (auto *entity_array = find_entity_array<EntityType>()) {
      return *entity_array;
    }

    auto const type_index =
      static_cast<std::size_t>(Entity_traits<EntityType>::type);
    if (_entity_arrays.size() <= type_index) {
      _entity_arrays.resize(type_index + 1);
    }
    auto entity_array = std::make_unique<Entity_array_impl<EntityType>>();
    auto *result = entity_array.get();
    _entity_arrays[type_index] = std::move(entity_array);
    return *result;
  }

  std::vector<std::unique_ptr<Entity_array>> _entity_arrays;
};

} // namespace fpsparty::game

#endif
