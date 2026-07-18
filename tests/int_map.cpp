#include <int_map.hpp>

#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

namespace {

using fpsparty::Int_map;
using fpsparty::Int_map_nz;

struct Tracked_value {
  static inline int live_count{};
  static inline int direct_construction_count{};

  explicit Tracked_value(int value) noexcept : value{value} {
    ++live_count;
    ++direct_construction_count;
  }

  Tracked_value(Tracked_value &&other) noexcept : value{other.value} {
    ++live_count;
  }

  Tracked_value(Tracked_value const &) = delete;
  Tracked_value &operator=(Tracked_value const &) = delete;
  Tracked_value &operator=(Tracked_value &&) = delete;

  ~Tracked_value() noexcept { --live_count; }

  int value;
};

} // namespace

TEST_CASE("Integer map inserts finds and preserves unique keys") {
  auto map = Int_map<std::int32_t, int>{};

  auto const [zero, zero_inserted] = map.try_emplace(0, 10);
  CHECK(zero_inserted);
  CHECK(zero->key() == 0);
  CHECK(zero->value() == 10);

  auto const [negative, negative_inserted] = map.try_emplace(-7, 20);
  auto const [duplicate, duplicate_inserted] = map.try_emplace(-7, 30);

  CHECK(negative_inserted);
  CHECK(negative->key() == -7);
  CHECK(negative->value() == 20);
  CHECK_FALSE(duplicate_inserted);
  CHECK(duplicate->value() == 20);
  CHECK(map.find(99) == map.end());

  negative->value() = 21;
  auto const &const_map = map;
  REQUIRE(const_map.find(-7) != const_map.end());
  CHECK(const_map.find(-7)->value() == 21);
}

TEST_CASE("Integer map growth reserve and rehash preserve 64-bit keys") {
  auto map = Int_map_nz<std::uint64_t, std::uint32_t>{};
  map.reserve(100);

  for (auto i = std::uint64_t{1}; i != 101; ++i) {
    auto const key = (i << 48) | (i * 0x1'0000'0001ull);
    auto const [position, inserted] =
      map.try_emplace(key, static_cast<std::uint32_t>(i));
    REQUIRE(inserted);
    CHECK(position->key() == key);
  }
  map.rehash(256);

  for (auto i = std::uint64_t{1}; i != 101; ++i) {
    auto const key = (i << 48) | (i * 0x1'0000'0001ull);
    REQUIRE(map.find(key) != map.end());
    CHECK(map.find(key)->key() == key);
    CHECK(map.find(key)->value() == i);
  }
}

TEST_CASE("Integer map supports a nonzero sentinel key") {
  auto map = Int_map<std::int32_t, int, -1>{};

  CHECK(map.try_emplace(0, 10).second);
  CHECK(map.try_emplace(-2, 20).second);
  CHECK(map.try_emplace(1, 30).second);
  map.reserve(100);
  map.rehash(256);

  REQUIRE(map.find(0) != map.end());
  CHECK(map.find(0)->value() == 10);
  REQUIRE(map.find(-2) != map.end());
  CHECK(map.find(-2)->value() == 20);
  CHECK(map.erase(0) == 1);
  CHECK(map.find(0) == map.end());
  REQUIRE(map.find(1) != map.end());
  CHECK(map.find(1)->value() == 30);
}

TEST_CASE("Integer map erasure closes probe clusters without tombstones") {
  auto map = Int_map_nz<std::uint32_t, std::uint32_t>{};
  for (auto key = std::uint32_t{1}; key != 80; ++key) {
    map.try_emplace(key, key * 10);
  }

  for (auto key = std::uint32_t{3}; key < 80; key += 3) {
    if (key % 2 == 0) {
      auto const position = map.find(key);
      REQUIRE(position != map.end());
      map.erase(position);
    } else {
      CHECK(map.erase(key) == 1);
    }
    CHECK(map.find(key) == map.end());
  }

  for (auto key = std::uint32_t{1}; key != 80; ++key) {
    if (key % 3 != 0) {
      REQUIRE(map.find(key) != map.end());
      CHECK(map.find(key)->value() == key * 10);
    }
  }
  CHECK(map.erase(200) == 0);
}

TEST_CASE("Integer map manages only the lifetimes of occupied values") {
  Tracked_value::live_count = 0;
  Tracked_value::direct_construction_count = 0;
  {
    auto map = Int_map<std::uint32_t, Tracked_value>{};
    CHECK(map.try_emplace(1, 10).second);
    CHECK(map.try_emplace(2, 20).second);
    auto const construction_count = Tracked_value::direct_construction_count;

    auto const [existing, inserted] = map.try_emplace(1, 30);
    CHECK_FALSE(inserted);
    CHECK(existing->value().value == 10);
    CHECK(Tracked_value::direct_construction_count == construction_count);

    map.reserve(200);
    CHECK(Tracked_value::live_count == 2);
    CHECK(map.erase(1) == 1);
    CHECK(Tracked_value::live_count == 1);
  }
  CHECK(Tracked_value::live_count == 0);
}
