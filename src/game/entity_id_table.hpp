#ifndef FPSPARTY_GAME_ENTITY_ID_TABLE_HPP
#define FPSPARTY_GAME_ENTITY_ID_TABLE_HPP

#include <bit>
#include <cassert>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <net/core/entity_id.hpp>

namespace fpsparty::game {
class Entity_id_table {
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

      constexpr bool empty() const noexcept {
        return key == 0;
      }
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
          temp.insert(bucket);
        }
      }
    }

    std::optional<std::uint32_t> lookup(std::uint32_t key) const noexcept {
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

    void insert(Entry entry) noexcept {
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

  private:
    static constexpr std::uint32_t hash(std::uint32_t key, int bits) noexcept {
      return (key * 2654435769u) >> (32 - bits);
    }

    std::unique_ptr<Entry[]> _buckets{};
    std::uint32_t _bucket_count{};
  };

private:
  Hash_table _id_to_index{};
  std::vector<std::uint32_t> _index_to_id{};
};
}

#endif
