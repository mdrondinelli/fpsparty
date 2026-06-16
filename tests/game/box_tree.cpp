#include "game/box_tree.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <math/vec.hpp>
#include <utility>
#include <vector>

namespace {

namespace game = fpsparty::game;
namespace math = fpsparty::math;

math::box3 box(math::vec3 min, math::vec3 max) { return {min, max}; }

std::vector<std::pair<int, int>> collect_pairs(game::Box_tree<int> &tree) {
  auto pairs = std::vector<std::pair<int, int>>{};
  tree.for_each_overlapping_leaf_pair([&](int lhs, int rhs) {
    if (rhs < lhs) {
      std::swap(lhs, rhs);
    }
    pairs.emplace_back(lhs, rhs);
  });
  std::ranges::sort(pairs);
  return pairs;
}

std::vector<std::pair<int, int>>
brute_force_pairs(std::vector<std::pair<math::box3, int>> const &leaves) {
  auto pairs = std::vector<std::pair<int, int>>{};
  for (auto i = std::size_t{}; i != leaves.size(); ++i) {
    for (auto j = i + 1; j != leaves.size(); ++j) {
      if (leaves[i].first.intersects(leaves[j].first)) {
        pairs.emplace_back(leaves[i].second, leaves[j].second);
      }
    }
  }
  std::ranges::sort(pairs);
  return pairs;
}

struct Destruction_counter {
  int value{};
  int *destruction_count{};

  Destruction_counter(int value, int &destruction_count)
      : value{value}, destruction_count{&destruction_count} {}

  Destruction_counter(Destruction_counter const &) = delete;
  Destruction_counter &operator=(Destruction_counter const &) = delete;

  Destruction_counter(Destruction_counter &&other) noexcept
      : value{other.value},
        destruction_count{std::exchange(other.destruction_count, nullptr)} {}

  Destruction_counter &operator=(Destruction_counter &&other) noexcept {
    value = other.value;
    destruction_count = std::exchange(other.destruction_count, nullptr);
    return *this;
  }

  ~Destruction_counter() {
    if (destruction_count != nullptr) {
      ++*destruction_count;
    }
  }
};

} // namespace

TEST_CASE("Box tree reports no pairs for empty and single-leaf trees") {
  auto tree = game::Box_tree<int>{};

  tree.build();
  CHECK(collect_pairs(tree).empty());

  tree.create_leaf(box({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}), 1);
  tree.build();

  CHECK(tree.leaf_count() == 1);
  CHECK_FALSE(tree.empty());
  CHECK(collect_pairs(tree).empty());
}

TEST_CASE("Box tree reports overlapping leaves once") {
  auto tree = game::Box_tree<int>{};
  tree.create_leaf(box({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f}), 1);
  tree.create_leaf(box({1.0f, 1.0f, 1.0f}, {3.0f, 3.0f, 3.0f}), 2);
  tree.create_leaf(box({5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}), 3);

  tree.build();

  CHECK(collect_pairs(tree) == std::vector<std::pair<int, int>>{{1, 2}});
}

TEST_CASE("Box tree recursive splits match brute force overlap results") {
  auto leaves = std::vector<std::pair<math::box3, int>>{
    {box({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f}), 1},
    {box({1.0f, 1.0f, 1.0f}, {3.0f, 3.0f, 3.0f}), 2},
    {box({4.0f, 0.0f, 0.0f}, {5.0f, 1.0f, 1.0f}), 3},
    {box({4.5f, 0.5f, 0.5f}, {6.0f, 2.0f, 2.0f}), 4},
    {box({8.0f, 8.0f, 8.0f}, {9.0f, 9.0f, 9.0f}), 5},
  };
  auto tree = game::Box_tree<int>{};
  for (auto const &[bounds, value] : leaves) {
    tree.create_leaf(bounds, value);
  }

  tree.build();

  CHECK(collect_pairs(tree) == brute_force_pairs(leaves));
}

TEST_CASE("Box tree coincident-centroid fallback matches brute force") {
  auto leaves = std::vector<std::pair<math::box3, int>>{
    {box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}), 1},
    {box({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}), 2},
    {box({-2.0f, -0.25f, -0.25f}, {2.0f, 0.25f, 0.25f}), 3},
    {box({-0.25f, -2.0f, -0.25f}, {0.25f, 2.0f, 0.25f}), 4},
  };
  auto tree = game::Box_tree<int>{};
  for (auto const &[bounds, value] : leaves) {
    tree.create_leaf(bounds, value);
  }

  tree.build();

  CHECK(collect_pairs(tree) == brute_force_pairs(leaves));
}

TEST_CASE("Box tree destroyed leaves are removed from rebuilt query results") {
  auto tree = game::Box_tree<int>{};
  tree.create_leaf(box({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f}), 1);
  auto const removed =
    tree.create_leaf(box({1.0f, 1.0f, 1.0f}, {3.0f, 3.0f, 3.0f}), 2);
  tree.create_leaf(box({1.5f, 1.5f, 1.5f}, {2.5f, 2.5f, 2.5f}), 3);
  tree.build();
  REQUIRE(collect_pairs(tree).size() == 3);

  tree.destroy_leaf(removed);
  tree.build();

  CHECK(collect_pairs(tree) == std::vector<std::pair<int, int>>{{1, 3}});
}

TEST_CASE("Box tree destroys payloads on leaf removal and clear") {
  auto destruction_count = 0;
  auto tree = game::Box_tree<Destruction_counter>{};
  auto const removed = tree.create_leaf(
    box({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}),
    Destruction_counter{1, destruction_count});
  tree.create_leaf(
    box({2.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 3.0f}),
    Destruction_counter{2, destruction_count});

  tree.destroy_leaf(removed);
  CHECK(destruction_count == 1);

  tree.clear();
  CHECK(destruction_count == 2);
  CHECK(tree.empty());
}
