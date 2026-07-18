#ifndef FPSPARTY_INT_MAP_HPP
#define FPSPARTY_INT_MAP_HPP

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include <int.hpp>

namespace fpsparty {

namespace detail {

template <std::size_t> struct Int_map_key_storage;

template <> struct Int_map_key_storage<sizeof(u32)> {
  template <std::integral K> void set(K key) noexcept {
    bits = std::bit_cast<u32>(key);
  }

  template <std::integral K> K get() const noexcept {
    return std::bit_cast<K>(bits);
  }

  u32 bits{};
};

template <> struct Int_map_key_storage<sizeof(u64)> {
  template <std::integral K> void set(K key) noexcept {
    auto const bits = std::bit_cast<u64>(key);
    low = static_cast<u32>(bits);
    high = static_cast<u32>(bits >> 32);
  }

  template <std::integral K> K get() const noexcept {
    auto const bits = static_cast<u64>(low) | (static_cast<u64>(high) << 32);
    return std::bit_cast<K>(bits);
  }

  u32 low{};
  u32 high{};
};

struct Int_map_occupancy {
  bool occupied{};
};

struct Int_map_no_occupancy {};

template <std::integral K> struct Int_map_sentinel {
  constexpr Int_map_sentinel() noexcept = default;

  constexpr Int_map_sentinel(K key) noexcept : enabled{true}, key{key} {}

  bool enabled{};
  K key{};
};

} // namespace detail

template <
  std::integral K,
  typename V,
  detail::Int_map_sentinel<K> sentinel_key = {}>
class Int_map {
  static_assert(sizeof(K) == sizeof(u32) || sizeof(K) == sizeof(u64));
  static_assert(std::is_nothrow_move_constructible_v<V>);
  static_assert(std::is_nothrow_destructible_v<V>);

  using Key_storage = detail::Int_map_key_storage<sizeof(K)>;
  using Occupancy = std::conditional_t<
    sentinel_key.enabled,
    detail::Int_map_no_occupancy,
    detail::Int_map_occupancy>;

public:
  using key_type = K;
  using mapped_type = V;
  using size_type = std::size_t;

  class Entry {
  public:
    Entry(Entry const &) = delete;
    Entry &operator=(Entry const &) = delete;
    Entry(Entry &&) = delete;
    Entry &operator=(Entry &&) = delete;

    ~Entry() noexcept {
      if (occupied()) {
        std::destroy_at(value_pointer());
      }
    }

    K key() const noexcept {
      assert(occupied());
      return stored_key();
    }

    V &value() noexcept {
      assert(occupied());
      return *value_pointer();
    }

    V const &value() const noexcept {
      assert(occupied());
      return *value_pointer();
    }

  private:
    friend class Int_map;

    Entry() noexcept {
      if constexpr (sentinel_key.enabled) {
        _key.set(sentinel_key.key);
      }
    }

    bool occupied() const noexcept {
      if constexpr (sentinel_key.enabled) {
        return stored_key() != sentinel_key.key;
      } else {
        return _occupancy.occupied;
      }
    }

    K stored_key() const noexcept { return _key.template get<K>(); }

    void set_key(K key) noexcept {
      _key.set(key);
      if constexpr (!sentinel_key.enabled) {
        _occupancy.occupied = true;
      }
    }

    void mark_empty() noexcept {
      if constexpr (sentinel_key.enabled) {
        _key.set(sentinel_key.key);
      } else {
        _key.set(K{});
        _occupancy.occupied = false;
      }
    }

    template <typename... Args>
      requires std::constructible_from<V, Args...>
    void emplace(K key, Args &&...args) {
      assert(!occupied());
      std::construct_at(value_pointer(), std::forward<Args>(args)...);
      set_key(key);
    }

    V *value_pointer() noexcept {
      return std::launder(reinterpret_cast<V *>(_value_storage));
    }

    V const *value_pointer() const noexcept {
      return std::launder(reinterpret_cast<V const *>(_value_storage));
    }

    Key_storage _key{};
    [[no_unique_address]] Occupancy _occupancy{};
    alignas(V) std::byte _value_storage[sizeof(V)];
  };

  static constexpr auto aligned_key_storage_size =
    (sizeof(Key_storage) + alignof(V) - 1) / alignof(V) * alignof(V);
  static_assert(
    !sentinel_key.enabled ||
    sizeof(Entry) == aligned_key_storage_size + sizeof(V),
    "sentinel-key entries must not contain occupancy or excess padding");

private:
  template <bool is_const> class Iterator {
    using Map = std::conditional_t<is_const, Int_map const, Int_map>;

  public:
    using value_type = Entry;
    using reference = std::conditional_t<is_const, Entry const &, Entry &>;
    using pointer = std::conditional_t<is_const, Entry const *, Entry *>;

    Iterator() noexcept = default;

    reference operator*() const noexcept {
      assert(_map != nullptr);
      assert(_index < _map->_bucket_count);
      assert(_map->_buckets[_index].occupied());
      return _map->_buckets[_index];
    }

    pointer operator->() const noexcept { return std::addressof(operator*()); }

    friend bool operator==(Iterator const &, Iterator const &) = default;

  private:
    friend class Int_map;

    Iterator(Map *map, size_type index) noexcept : _map{map}, _index{index} {}

    Map *_map{};
    size_type _index{};
  };

public:
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;

  Int_map() noexcept = default;

  Int_map(Int_map const &) = delete;
  Int_map &operator=(Int_map const &) = delete;

  Int_map(Int_map &&other) noexcept
      : _buckets{std::move(other._buckets)},
        _bucket_count{std::exchange(other._bucket_count, 0)},
        _size{std::exchange(other._size, 0)} {}

  Int_map &operator=(Int_map &&other) noexcept {
    auto replacement = std::move(other);
    swap(replacement);
    return *this;
  }

  iterator find(K key) noexcept {
    return iterator{this, find_index(key)};
  }

  const_iterator find(K key) const noexcept {
    return const_iterator{this, find_index(key)};
  }

  iterator end() noexcept { return iterator{this, _bucket_count}; }

  const_iterator end() const noexcept {
    return const_iterator{this, _bucket_count};
  }

  template <typename... Args>
    requires std::constructible_from<V, Args...>
  std::pair<iterator, bool> try_emplace(K key, Args &&...args) {
    assert_valid_key(key);
    if (auto const existing = find(key); existing != end()) {
      return {existing, false};
    }

    auto candidate = Entry{};
    candidate.emplace(key, std::forward<Args>(args)...);
    reserve(_size + 1);
    auto const index = insert_without_growth(candidate);
    return {iterator{this, index}, true};
  }

  size_type erase(K key) noexcept {
    auto const position = find(key);
    if (position == end()) {
      return 0;
    }
    erase(position);
    return 1;
  }

  void erase(iterator position) noexcept {
    assert(position._map == this);
    assert(position._index < _bucket_count);
    assert(_buckets[position._index].occupied());
    erase_index(position._index);
  }

  void reserve(size_type entry_count) {
    if (entry_count <= max_entry_count(_bucket_count)) {
      return;
    }

    auto bucket_count = _bucket_count == 0 ? min_bucket_count : _bucket_count;
    while (entry_count > max_entry_count(bucket_count)) {
      assert(bucket_count <= maximum_bucket_count() / 2);
      bucket_count *= 2;
    }
    rehash(bucket_count);
  }

  void rehash(size_type bucket_count) {
    assert(bucket_count >= min_bucket_count);
    assert(std::has_single_bit(bucket_count));
    assert(bucket_count <= maximum_bucket_count());
    assert(_size <= max_entry_count(bucket_count));
    if (bucket_count == _bucket_count) {
      return;
    }

    auto replacement = Int_map{};
    replacement._buckets = allocate_buckets(bucket_count);
    replacement._bucket_count = bucket_count;
    for (auto i = size_type{}; i != _bucket_count; ++i) {
      auto &entry = _buckets[i];
      if (entry.occupied()) {
        auto candidate = Entry{};
        candidate.emplace(entry.key(), std::move(entry.value()));
        replacement.insert_without_growth(candidate);
      }
    }
    swap(replacement);
  }

private:
  static constexpr size_type min_bucket_count = 2;
  static constexpr size_type load_numerator = 85;
  static constexpr size_type load_denominator = 100;

  static constexpr size_type max_entry_count(size_type bucket_count) noexcept {
    return (bucket_count / load_denominator) * load_numerator +
           ((bucket_count % load_denominator) * load_numerator) /
             load_denominator;
  }

  static constexpr size_type maximum_bucket_count() noexcept {
    return std::bit_floor(
      std::numeric_limits<size_type>::max() / sizeof(Entry));
  }

  static std::unique_ptr<Entry[]> allocate_buckets(size_type bucket_count) {
    return std::unique_ptr<Entry[]>{new Entry[bucket_count]};
  }

  static void assert_valid_key(K key) noexcept {
    if constexpr (sentinel_key.enabled) {
      assert(key != sentinel_key.key);
    }
  }

  static size_type hash(K key, size_type bucket_count) noexcept {
    auto const bits = std::bit_width(bucket_count - 1);
    if constexpr (sizeof(K) == sizeof(u32)) {
      auto const value = std::bit_cast<u32>(key);
      return (value * 2654435769u) >> (32 - bits);
    } else {
      auto value = std::bit_cast<u64>(key);
      auto const shift = 64 - bits;
      value ^= value >> shift;
      return (value * 11400714819323198485ull) >> shift;
    }
  }

  size_type probe_distance(K key, size_type index) const noexcept {
    auto const mask = _bucket_count - 1;
    return (index - hash(key, _bucket_count)) & mask;
  }

  size_type find_index(K key) const noexcept {
    assert_valid_key(key);
    if (_bucket_count == 0) {
      return 0;
    }

    auto const mask = _bucket_count - 1;
    auto index = hash(key, _bucket_count);
    auto distance = size_type{};
    for (;;) {
      auto const &entry = _buckets[index];
      if (!entry.occupied() || probe_distance(entry.key(), index) < distance) {
        return _bucket_count;
      }
      if (entry.key() == key) {
        return index;
      }
      index = (index + 1) & mask;
      ++distance;
    }
  }

  static void relocate(Entry &destination, Entry &source) noexcept {
    assert(!destination.occupied());
    assert(source.occupied());
    auto const key = source.key();
    std::construct_at(
      destination.value_pointer(), std::move(source.value()));
    destination.set_key(key);
    std::destroy_at(source.value_pointer());
    source.mark_empty();
  }

  static void swap_occupied(Entry &a, Entry &b) noexcept {
    assert(a.occupied());
    assert(b.occupied());
    auto temporary = Entry{};
    relocate(temporary, a);
    relocate(a, b);
    relocate(b, temporary);
  }

  size_type insert_without_growth(Entry &candidate) noexcept {
    assert(candidate.occupied());
    assert(_bucket_count != 0);
    auto const mask = _bucket_count - 1;
    auto index = hash(candidate.key(), _bucket_count);
    auto distance = size_type{};
    auto inserted_index = _bucket_count;
    for (;;) {
      auto &bucket = _buckets[index];
      if (!bucket.occupied()) {
        relocate(bucket, candidate);
        ++_size;
        return inserted_index == _bucket_count ? index : inserted_index;
      }

      assert(bucket.key() != candidate.key());
      auto const bucket_distance = probe_distance(bucket.key(), index);
      if (bucket_distance < distance) {
        swap_occupied(bucket, candidate);
        if (inserted_index == _bucket_count) {
          inserted_index = index;
        }
        distance = bucket_distance;
      }
      index = (index + 1) & mask;
      ++distance;
      assert(distance < _bucket_count);
    }
  }

  void erase_index(size_type index) noexcept {
    auto &erased = _buckets[index];
    std::destroy_at(erased.value_pointer());
    erased.mark_empty();

    auto const mask = _bucket_count - 1;
    for (;;) {
      auto const next = (index + 1) & mask;
      auto &next_entry = _buckets[next];
      if (!next_entry.occupied() || probe_distance(next_entry.key(), next) == 0) {
        break;
      }
      relocate(_buckets[index], next_entry);
      index = next;
    }
    --_size;
  }

  void swap(Int_map &other) noexcept {
    _buckets.swap(other._buckets);
    std::swap(_bucket_count, other._bucket_count);
    std::swap(_size, other._size);
  }

  std::unique_ptr<Entry[]> _buckets{};
  size_type _bucket_count{};
  size_type _size{};
};

template <std::integral K, typename V>
using Int_map_nz = Int_map<K, V, K{0}>;

} // namespace fpsparty

#endif
