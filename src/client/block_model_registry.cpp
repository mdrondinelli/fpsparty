#include "block_model_registry.hpp"

#include <bit>

namespace fpsparty::client {

namespace {

class Hash_table {
public:
  using Key = std::pair<game::Block, u32>;

  struct Entry {
    Key key{};
    std::unique_ptr<Block_model> value{};

    constexpr Entry() noexcept = default;

    constexpr Entry(Key key, Block_model &&value)
        : key{key}, value{std::make_unique<Block_model>(std::move(value))} {}

    constexpr bool empty() const noexcept { return !value; }
  };

  constexpr Hash_table() noexcept = default;

  explicit Hash_table(u64 bucket_count)
      : _buckets(std::make_unique<Entry[]>(bucket_count)),
        _bucket_count{bucket_count} {
    assert(bucket_count >= min_bucket_count);
    assert(std::has_single_bit(bucket_count));
  }

  Hash_table(Hash_table &&other) noexcept
      : _buckets{std::move(other._buckets)},
        _bucket_count{std::exchange(other._bucket_count, 0)} {}

  Hash_table &operator=(Hash_table &&other) noexcept {
    auto temp = std::move(other);
    swap(temp);
    return *this;
  }

  void rehash(u64 new_bucket_count) {
    auto replacement = Hash_table{new_bucket_count};
    auto live_entry_count = u64{};
    for (auto const &bucket : buckets()) {
      if (!bucket.empty()) {
        ++live_entry_count;
        assert(new_bucket_count > live_entry_count);
        replacement.insert(bucket.key, std::move(*bucket.value));
      }
    }
    swap(replacement);
  }

  void insert(Key key, Block_model &&value) {
    auto const predicted_load_factor =
      static_cast<f64>(_entry_count + 1) / _bucket_count;
    if (predicted_load_factor > max_load_factor) {
      rehash(bucket_count() * 2);
    }
    insert_impl(key, std::move(value));
  }

  Block_model const *get(Key key) const noexcept {
    if (_bucket_count == 0) {
      return nullptr;
    }
    auto const mask = _bucket_count - 1;
    auto const bits = std::countr_one(mask);
    auto i = hash(key, bits);
    for (;;) {
      auto const &entry = _buckets[i];
      if (entry.empty()) {
        return nullptr;
      }
      if (entry.key == key) {
        return entry.value.get();
      }
      i = (i + 1) & mask;
    }
  }

  u64 bucket_count() const noexcept { return _bucket_count; }

  static auto constexpr min_bucket_count = u64{1} << 1;
  static auto constexpr max_load_factor = 0.85;

private:
  static constexpr u64 hash(Key key, int bits) noexcept {
    auto const shift = 64 - bits;
    auto k = static_cast<u64>(pack_block_data(key.first, key.second));
    k ^= k >> shift;
    return (k * 11400714819323198485llu) >> shift;
  }

  void insert_impl(Key key, Block_model &&value) {
    auto entry = Entry{key, std::move(value)};
    auto const mask = _bucket_count - 1;
    auto const bits = std::countr_one(mask);
    auto i = hash(key, bits);
    auto distance = u64{};
    for (;;) {
      auto &bucket = _buckets[i];
      if (bucket.empty()) {
        bucket = std::move(entry);
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

  void swap(Hash_table &other) noexcept {
    _buckets.swap(other._buckets);
    std::swap(_bucket_count, other._bucket_count);
  }

  std::span<Entry const> buckets() const noexcept {
    return {_buckets.get(), _bucket_count};
  }

  std::unique_ptr<Entry[]> _buckets{};
  u64 _bucket_count{};
  u64 _entry_count{};
};

} // namespace

struct Block_model_registry::Impl {
  Hash_table table{8};
};

Block_model_registry::Block_model_registry()
    : _impl{std::make_unique<Impl>()} {}

Block_model_registry::~Block_model_registry() = default;

void Block_model_registry::add(
  game::Block block, int data, Block_model &&model) {
  _impl->table.insert({block, data}, std::move(model));
}

Block_model const *
Block_model_registry::get(game::Block block, int data) const {
  return _impl->table.get({block, data});
}

} // namespace fpsparty::client
