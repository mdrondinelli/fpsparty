#ifndef FPSPARTY_GAME_ENTITY_WORLD_HPP
#define FPSPARTY_GAME_ENTITY_WORLD_HPP

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <net/entity_id.hpp>

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
        // if you're trying to construct the empty key, use the default ctor
        assert(key != 0);
      }

      constexpr bool empty() const noexcept { return key == 0; }

      constexpr void reset() noexcept { key = 0; }
    };

    explicit Hash_table(std::uint32_t bucket_count)
        : _buckets(std::make_unique<Entry[]>(bucket_count)),
          _bucket_count{bucket_count} {
      assert(bucket_count >= min_bucket_count);
      assert(std::has_single_bit(bucket_count));
    }

    void rehash(std::uint32_t new_bucket_count) {
      assert(new_bucket_count >= min_bucket_count);
      assert(std::has_single_bit(new_bucket_count));
      auto replacement = Hash_table{new_bucket_count};
      auto live_entry_count = std::uint32_t{};
      for (auto const &bucket : buckets()) {
        if (!bucket.empty()) {
          ++live_entry_count;
          assert(new_bucket_count > live_entry_count);
          replacement.push(bucket);
        }
      }
      swap(replacement);
    }

    std::optional<std::uint32_t> get(std::uint32_t key) const noexcept {
      assert(key != 0);
      assert(_bucket_count != 0);
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

    void push(Entry entry) noexcept {
      assert(!entry.empty());
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(entry.key, bits);
      auto distance = std::uint32_t{};
      for (;;) {
        auto &bucket = _buckets[i];
        if (bucket.empty()) {
          bucket = entry;
          return;
        }
        assert(bucket.key != entry.key && "double inserts not allowed");
        auto const bucket_hash = hash(bucket.key, bits);
        auto const bucket_distance = (i - bucket_hash) & mask;
        if (bucket_distance < distance) {
          std::swap(bucket, entry);
          distance = bucket_distance;
        }
        i = (i + 1) & mask;
        ++distance;
        assert(distance < _bucket_count && "must not insert into full table");
      }
    }

    void push(std::uint32_t key, std::uint32_t value) noexcept {
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
        assert(!bucket.empty() && "pop key must be in table");
        if (bucket.key == key) {
          auto const result = bucket.value;
          for (;;) {
            auto const j = (i + 1) & mask;
            auto &next_bucket = _buckets[j];
            if (next_bucket.empty() || hash(next_bucket.key, bits) == j) {
              _buckets[i].reset();
              return result;
            }
            _buckets[i] = next_bucket;
            i = j;
          }
        }
        i = (i + 1) & mask;
      }
    }

    std::uint32_t exchange(std::uint32_t key, std::uint32_t value) noexcept {
      assert(key != 0);
      assert(_bucket_count != 0);
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      for (;;) {
        auto &bucket = _buckets[i];
        assert(!bucket.empty() && "exhange key must be in table");
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

    static constexpr std::uint32_t min_bucket_count = 2;

    static constexpr std::uint32_t max_bucket_count = 0x80000000u;

  private:
    static constexpr std::uint32_t hash(std::uint32_t key, int bits) noexcept {
      return (key * 2654435769u) >> (32 - bits);
    }

    void swap(Hash_table &other) noexcept {
      _buckets.swap(other._buckets);
      std::swap(_bucket_count, other._bucket_count);
    }

    std::unique_ptr<Entry[]> _buckets{};
    std::uint32_t _bucket_count{};
  };

private:
  struct Entity_array {
    virtual ~Entity_array() = default;
  };

  template <typename EntityType>
  struct Entity_array_impl : public Entity_array {
    void erase_index(std::size_t index) noexcept {
      assert(index < entities.size());
      index_lookup_table.pop(entity_ids[index]);
      erase_popped_index(index);
    }

    void erase_id(net::Entity_id id) noexcept {
      erase_popped_index(index_lookup_table.pop(id));
    }

  private:
    void erase_popped_index(std::size_t index) noexcept {
      auto const last_index = entities.size() - 1;
      if (index != last_index) {
        entities[index] = std::move(entities.back());
        auto const moved_entity_id = entity_ids.back();
        entity_ids[index] = moved_entity_id;
        index_lookup_table.exchange(moved_entity_id, index);
      }
      entities.pop_back();
      entity_ids.pop_back();
    }

  public:
    std::vector<EntityType> entities;
    std::vector<net::Entity_id> entity_ids;
    Hash_table index_lookup_table{8};
    net::Entity_id *next_entity_id{};
  };

  template <typename EntityType> struct Entry {
    EntityType &entity;
    Entity_handle<EntityType> handle;
  };

public:
  template <typename EntityType> class Entity_range {
  public:
    using Base_entity_type = std::remove_const_t<EntityType>;
    using Array_type = Entity_array_impl<Base_entity_type>;
    using Array_pointer = std::conditional_t<
      std::is_const_v<EntityType>,
      Array_type const *,
      Array_type *>;

    class Iterator {
    public:
      using difference_type = std::ptrdiff_t;
      using value_type = Entry<EntityType>;
      using iterator_category = std::input_iterator_tag;
      using iterator_concept = std::input_iterator_tag;

      constexpr EntityType &entity() const noexcept {
        return _entity_array->entities[_index];
      }

      constexpr Entity_handle<EntityType> handle() const noexcept {
        return Entity_handle<EntityType>{_entity_array->entity_ids[_index]};
      }

      constexpr value_type operator*() const noexcept {
        return {entity(), handle()};
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

      constexpr Iterator(Array_pointer entity_array, std::size_t index) noexcept
          : _entity_array{entity_array}, _index{index} {}

      Array_pointer _entity_array{};
      std::size_t _index{};
    };

    constexpr Iterator begin() const noexcept { return {_entity_array, 0}; }

    constexpr Iterator end() const noexcept {
      return {_entity_array, _entity_array->entities.size()};
    }

    constexpr Entry<EntityType> operator[](std::size_t index) const noexcept {
      return {
        _entity_array->entities[index],
        Entity_handle<EntityType>{_entity_array->entity_ids[index]},
      };
    }

    constexpr std::size_t size() const noexcept {
      return _entity_array->entities.size();
    }

    // Invalidates all iterators.
    Entry<EntityType> insert(EntityType &&entity) const {
      return emplace(std::move(entity));
    }

    // Invalidates all iterators.
    template <typename... Args>
      requires(!std::is_const_v<EntityType>)
    Entry<EntityType> emplace(Args &&...args) const {
      auto const current_bucket_count =
        _entity_array->index_lookup_table.buckets().size();
      auto const max_entry_count = current_bucket_count * 0.85;
      auto const required_entry_count = _entity_array->entities.size() + 1;
      if (required_entry_count > max_entry_count) {
        assert(current_bucket_count != Hash_table::max_bucket_count);
        _entity_array->index_lookup_table.rehash(current_bucket_count * 2);
      }
      auto &entity =
        _entity_array->entities.emplace_back(std::forward<Args>(args)...);
      auto const id = (*_entity_array->next_entity_id)++;
      assert(id != 0);
      _entity_array->entity_ids.push_back(id);
      _entity_array->index_lookup_table.push(
        id, static_cast<std::uint32_t>(_entity_array->entities.size() - 1));
      return {entity, Entity_handle<EntityType>{id}};
    }

    Iterator find(Entity_handle<EntityType> handle) const noexcept {
      if (!handle) {
        return end();
      }
      auto const index = _entity_array->index_lookup_table.get(handle.id);
      return index ? Iterator{_entity_array, *index} : end();
    }

    EntityType *get(Entity_handle<EntityType> handle) const noexcept {
      if (!handle) {
        return nullptr;
      }
      auto const index = _entity_array->index_lookup_table.get(handle.id);
      return index ? &_entity_array->entities[*index] : nullptr;
    }

    // Invalidates all iterators.
    // Returns the iterator to the next in iteration order.
    Iterator erase(Iterator position) const noexcept
      requires(!std::is_const_v<EntityType>)
    {
      assert(position._entity_array == _entity_array);
      assert(position._index < size());
      auto const index = position._index;
      _entity_array->erase_index(index);
      return {_entity_array, index};
    }

  private:
    friend class Entity_world;

    explicit constexpr Entity_range(Array_pointer entity_array) noexcept
        : _entity_array{entity_array} {
      assert(
        _entity_array->entities.size() == _entity_array->entity_ids.size());
    }

    Array_pointer _entity_array;
  };

  // Inavlidates iterators for all entities of type EntityType.
  template <typename EntityType, typename... Args>
    requires(!std::is_const_v<EntityType>)
  Entry<EntityType> insert(EntityType &&entity) {
    return get_all<EntityType>().insert(std::move(entity));
  }

  // Inavlidates iterators for all entities of type EntityType.
  template <typename EntityType, typename... Args>
    requires(!std::is_const_v<EntityType>)
  Entry<EntityType> emplace(Args &&...args) {
    return get_all<EntityType>().emplace(std::forward<Args>(args)...);
  }

  // Inavlidates iterators for all entities of type EntityType.
  template <typename EntityType>
    requires(!std::is_const_v<EntityType>)
  bool remove(Entity_handle<EntityType> handle) {
    auto const entities = get_all<EntityType>();
    auto const it = entities.find(handle);
    if (it != entities.end()) {
      entities.erase(it);
      return true;
    } else {
      return false;
    }
  }

  template <typename EntityType>
  EntityType const *get(Entity_handle<EntityType const> handle) const noexcept {
    if (!handle) {
      return nullptr;
    }
    auto const entity_array = find_entity_array<EntityType>();
    assert(entity_array && "entity type must be registered");
    auto const index = entity_array->index_lookup_table.get(handle.id);
    return index ? &entity_array->entities[*index] : nullptr;
  }

  template <typename EntityType>
    requires(!std::is_const_v<EntityType>)
  EntityType *get(Entity_handle<EntityType> handle) noexcept {
    return const_cast<EntityType *>(
      get(Entity_handle<EntityType const>{handle}));
  }

  template <typename EntityType>
  Entity_range<EntityType const> get_all() const noexcept {
    auto const entity_array = find_entity_array<EntityType>();
    assert(entity_array && "entity type must be registered");
    return Entity_range<EntityType const>{entity_array};
  }

  template <typename EntityType> Entity_range<EntityType> get_all() noexcept {
    auto const entity_array = find_entity_array<EntityType>();
    assert(entity_array && "entity type must be registered");
    return Entity_range<EntityType>{entity_array};
  }

  template <typename EntityType>
    requires(!std::is_const_v<EntityType>)
  void register_entity_type() {
    auto constexpr type_index =
      static_cast<std::size_t>(Entity_traits<EntityType>::type);
    if (_entity_arrays.size() <= type_index) {
      _entity_arrays.resize(type_index + 1);
    }
    auto entity_array = std::make_unique<Entity_array_impl<EntityType>>();
    entity_array->next_entity_id = _next_entity_id.get();
    _entity_arrays[type_index] = std::move(entity_array);
  }

private:
  template <typename EntityType>
    requires(!std::is_const_v<EntityType>)
  Entity_array_impl<EntityType> const *find_entity_array() const noexcept {
    auto const type_index =
      static_cast<std::size_t>(Entity_traits<EntityType>::type);
    if (type_index >= _entity_arrays.size()) {
      return nullptr;
    }
    return static_cast<Entity_array_impl<EntityType> *>(
      _entity_arrays[type_index].get());
  }

  template <typename EntityType>
    requires(!std::is_const_v<EntityType>)
  Entity_array_impl<EntityType> *find_entity_array() noexcept {
    return const_cast<Entity_array_impl<EntityType> *>(
      std::as_const(*this).find_entity_array<EntityType>());
  }

  std::vector<std::unique_ptr<Entity_array>> _entity_arrays;
  std::unique_ptr<net::Entity_id> _next_entity_id{std::make_unique<net::Entity_id>(1)};
};

} // namespace fpsparty::game

#endif
