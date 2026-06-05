#include "game/entity_id_table.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>

namespace {
using Hash_table = fpsparty::game::Entity_id_table::Hash_table;
using Entry = Hash_table::Entry;

std::uint32_t bucket_key(Hash_table const &table, std::size_t index) {
  return table.buckets()[index].key;
}

std::uint32_t bucket_value(Hash_table const &table, std::size_t index) {
  return table.buckets()[index].value;
}

std::size_t live_bucket_count(Hash_table const &table) {
  auto const buckets = table.buckets();
  return static_cast<std::size_t>(std::count_if(
    buckets.begin(),
    buckets.end(),
    [](Entry const &entry) {
      return !entry.empty();
    }));
}
} // namespace

TEST_CASE("Entity id hash table starts empty") {
  auto table = Hash_table{8};

  REQUIRE(table.buckets().size() == 8);
  CHECK(live_bucket_count(table) == 0);
  CHECK_FALSE(table.lookup(1));
}

TEST_CASE("Entity id hash table inserts and looks up entries") {
  auto table = Hash_table{8};

  table.insert({1, 10});
  table.insert({2, 20});
  table.insert({3, 30});

  REQUIRE(table.lookup(1));
  CHECK(*table.lookup(1) == 10);
  REQUIRE(table.lookup(2));
  CHECK(*table.lookup(2) == 20);
  REQUIRE(table.lookup(3));
  CHECK(*table.lookup(3) == 30);
  CHECK_FALSE(table.lookup(4));
  CHECK(live_bucket_count(table) == 3);
}

TEST_CASE("Entity id hash table resolves linear-probed collisions") {
  auto table = Hash_table{8};

  table.insert({1, 10});
  table.insert({9, 90});
  table.insert({17, 170});

  REQUIRE(table.lookup(1));
  CHECK(*table.lookup(1) == 10);
  REQUIRE(table.lookup(9));
  CHECK(*table.lookup(9) == 90);
  REQUIRE(table.lookup(17));
  CHECK(*table.lookup(17) == 170);

  CHECK(bucket_key(table, 4) == 1);
  CHECK(bucket_value(table, 4) == 10);
  CHECK(bucket_key(table, 5) == 9);
  CHECK(bucket_value(table, 5) == 90);
  CHECK(bucket_key(table, 6) == 17);
  CHECK(bucket_value(table, 6) == 170);
}

TEST_CASE("Entity id hash table uses Robin Hood displacement") {
  auto table = Hash_table{8};

  table.insert({1, 10});
  table.insert({6, 60});
  table.insert({9, 90});

  REQUIRE(table.lookup(1));
  CHECK(*table.lookup(1) == 10);
  REQUIRE(table.lookup(6));
  CHECK(*table.lookup(6) == 60);
  REQUIRE(table.lookup(9));
  CHECK(*table.lookup(9) == 90);

  CHECK(bucket_key(table, 4) == 1);
  CHECK(bucket_value(table, 4) == 10);
  CHECK(bucket_key(table, 5) == 9);
  CHECK(bucket_value(table, 5) == 90);
  CHECK(bucket_key(table, 6) == 6);
  CHECK(bucket_value(table, 6) == 60);
}

TEST_CASE("Entity id hash table erases entries from a collision chain") {
  auto table = Hash_table{8};

  table.insert({1, 10});
  table.insert({9, 90});
  table.insert({17, 170});

  table.erase(9);

  REQUIRE(table.lookup(1));
  CHECK(*table.lookup(1) == 10);
  CHECK_FALSE(table.lookup(9));
  REQUIRE(table.lookup(17));
  CHECK(*table.lookup(17) == 170);

  CHECK(bucket_key(table, 4) == 1);
  CHECK(bucket_key(table, 5) == 17);
  CHECK(table.buckets()[6].empty());
}

TEST_CASE("Entity id hash table erases the final entry in a collision chain") {
  auto table = Hash_table{8};

  table.insert({1, 10});
  table.insert({9, 90});
  table.insert({17, 170});

  table.erase(17);

  REQUIRE(table.lookup(1));
  CHECK(*table.lookup(1) == 10);
  REQUIRE(table.lookup(9));
  CHECK(*table.lookup(9) == 90);
  CHECK_FALSE(table.lookup(17));

  CHECK(bucket_key(table, 4) == 1);
  CHECK(bucket_key(table, 5) == 9);
  CHECK(table.buckets()[6].empty());
}

TEST_CASE("Entity id hash table rehash preserves entries") {
  auto table = Hash_table{8};

  table.insert({1, 10});
  table.insert({9, 90});
  table.insert({17, 170});

  table.rehash(16);

  REQUIRE(table.buckets().size() == 16);
  REQUIRE(table.lookup(1));
  CHECK(*table.lookup(1) == 10);
  REQUIRE(table.lookup(9));
  CHECK(*table.lookup(9) == 90);
  REQUIRE(table.lookup(17));
  CHECK(*table.lookup(17) == 170);
  CHECK_FALSE(table.lookup(2));
}
