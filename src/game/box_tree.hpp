#ifndef FPSPARTY_GAME_BOX_TREE_HPP
#define FPSPARTY_GAME_BOX_TREE_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <math/box.hpp>

namespace fpsparty::game {

template <typename Payload> class Box_tree {
public:
  struct Node {
    math::box3 bounds{};
    std::array<Node *, 2> children{};
    std::optional<Payload> payload{};

    bool is_leaf() const noexcept { return payload.has_value(); }
  };

  explicit Box_tree(
    std::pmr::memory_resource *upstream_memory_resource =
      std::pmr::get_default_resource())
      : _pool{upstream_memory_resource}, _node_allocator{&_pool} {}

  Box_tree(Box_tree const &) = delete;
  Box_tree &operator=(Box_tree const &) = delete;
  Box_tree(Box_tree &&) = delete;
  Box_tree &operator=(Box_tree &&) = delete;

  ~Box_tree() { clear(); }

  Node *create_leaf(math::box3 const &bounds, Payload payload) {
    auto const node = allocate_node();
    try {
      std::construct_at(
        node,
        Node{
          .bounds = bounds,
          .payload = std::move(payload),
        });
    } catch (...) {
      deallocate_node(node);
      throw;
    }
    try {
      _leaf_nodes.push_back(node);
    } catch (...) {
      destroy_node(node);
      throw;
    }
    clear_internal_nodes();
    return node;
  }

  void destroy_leaf(Node *leaf) noexcept {
    assert(leaf != nullptr);
    assert(leaf->is_leaf());
    auto const it = std::ranges::find(_leaf_nodes, leaf);
    assert(it != _leaf_nodes.end());
    clear_internal_nodes();
    _leaf_nodes.erase(it);
    destroy_node(leaf);
  }

  void clear() noexcept {
    clear_internal_nodes();
    for (auto const leaf : _leaf_nodes) {
      destroy_node(leaf);
    }
    _leaf_nodes.clear();
    _scratch_leaf_nodes.clear();
  }

  void build() {
    clear_internal_nodes();
    if (_leaf_nodes.empty()) {
      return;
    }
    if (_leaf_nodes.size() == 1) {
      _root_node = _leaf_nodes.front();
      return;
    }
    _internal_nodes.reserve(_leaf_nodes.size() - 1);
    _scratch_leaf_nodes = _leaf_nodes;
    _root_node = build_internal_node(_scratch_leaf_nodes);
  }

  std::size_t leaf_count() const noexcept { return _leaf_nodes.size(); }

  bool empty() const noexcept { return _leaf_nodes.empty(); }

  template <typename F>
  void for_each_overlapping_leaf_pair(F &&f) noexcept(
    noexcept(f(std::declval<Payload &>(), std::declval<Payload &>()))) {
    if (_root_node != nullptr) {
      for_each_overlapping_leaf_pair(_root_node, std::forward<F>(f));
    }
  }

  template <typename F>
  void for_each_overlapping_leaf_pair(F &&f) const noexcept(noexcept(
    f(std::declval<Payload const &>(), std::declval<Payload const &>()))) {
    if (_root_node != nullptr) {
      for_each_overlapping_leaf_pair(_root_node, std::forward<F>(f));
    }
  }

private:
  Node *allocate_node() { return _node_allocator.allocate(1); }

  void deallocate_node(Node *node) noexcept {
    _node_allocator.deallocate(node, 1);
  }

  void destroy_node(Node *node) noexcept {
    std::destroy_at(node);
    deallocate_node(node);
  }

  Node *create_internal_node(math::box3 const &bounds) {
    auto const node = allocate_node();
    try {
      std::construct_at(node, Node{.bounds = bounds});
    } catch (...) {
      deallocate_node(node);
      throw;
    }
    try {
      _internal_nodes.push_back(node);
    } catch (...) {
      destroy_node(node);
      throw;
    }
    return node;
  }

  void clear_internal_nodes() noexcept {
    _root_node = nullptr;
    for (auto const node : _internal_nodes) {
      destroy_node(node);
    }
    _internal_nodes.clear();
  }

  Node *build_internal_node(std::span<Node *> leaf_nodes) {
    assert(leaf_nodes.size() > 1);
    auto bounds = leaf_nodes.front()->bounds;
    for (auto const leaf_node : leaf_nodes.subspan(1)) {
      bounds.extend(leaf_node->bounds);
    }
    auto const node = create_internal_node(bounds);
    auto const node_extents = bounds.diagonal();
    auto axis_indices = std::array{0, 1, 2};
    std::ranges::sort(axis_indices, [&](int lhs, int rhs) {
      return node_extents[lhs] > node_extents[rhs];
    });
    for (auto const axis_index : axis_indices) {
      auto const split_position = [&] {
        auto accumulator = 0.0f;
        for (auto const leaf_node : leaf_nodes) {
          accumulator += leaf_node->bounds.min()[axis_index] +
                         leaf_node->bounds.max()[axis_index];
        }
        return accumulator / (2.0f * static_cast<float>(leaf_nodes.size()));
      }();
      auto const partition_it =
        partition(leaf_nodes, axis_index, split_position);
      auto const partitions = std::array{
        std::span{leaf_nodes.begin(), partition_it},
        std::span{partition_it, leaf_nodes.end()},
      };
      if (!partitions[0].empty() && !partitions[1].empty()) {
        node->children = {
          partitions[0].size() == 1 ? partitions[0].front()
                                    : build_internal_node(partitions[0]),
          partitions[1].size() == 1 ? partitions[1].front()
                                    : build_internal_node(partitions[1]),
        };
        return node;
      }
    }
    build_coincident_centroid_node(node, leaf_nodes);
    return node;
  }

  static auto partition(
    std::span<Node *> leaf_nodes, int axis_index, float split_position) {
    assert(!leaf_nodes.empty());
    auto const double_split_position = 2.0f * split_position;
    auto const double_position = [axis_index](Node const *node) {
      return node->bounds.min()[axis_index] + node->bounds.max()[axis_index];
    };
    auto unpartitioned_begin = leaf_nodes.begin();
    auto unpartitioned_end = leaf_nodes.end();
    do {
      auto &unpartitioned_leaf_node = *unpartitioned_begin;
      if (double_position(unpartitioned_leaf_node) < double_split_position) {
        ++unpartitioned_begin;
      } else {
        --unpartitioned_end;
        std::swap(unpartitioned_leaf_node, *unpartitioned_end);
      }
    } while (unpartitioned_begin != unpartitioned_end);
    return unpartitioned_begin;
  }

  void
  build_coincident_centroid_node(Node *node, std::span<Node *> leaf_nodes) {
    assert(leaf_nodes.size() > 1);
    std::ranges::sort(leaf_nodes, [](Node const *lhs, Node const *rhs) {
      return lhs->bounds.volume() < rhs->bounds.volume();
    });
    auto left_node = leaf_nodes[0];
    auto right_it = leaf_nodes.begin() + 1;
    for (;;) {
      auto const right_node = *right_it;
      if (++right_it == leaf_nodes.end()) {
        node->children = {left_node, right_node};
        return;
      }
      auto parent_bounds = left_node->bounds;
      parent_bounds.extend(right_node->bounds);
      auto const parent_node = create_internal_node(parent_bounds);
      parent_node->children = {left_node, right_node};
      left_node = parent_node;
    }
  }

  template <typename F>
  static void for_each_overlapping_leaf_pair(Node *root, F &&f) noexcept(
    noexcept(f(std::declval<Payload &>(), std::declval<Payload &>()))) {
    assert(root != nullptr);
    if (!root->is_leaf()) {
      auto const &children = root->children;
      for_each_overlapping_leaf_pair(
        children[0], children[1], std::forward<F>(f));
      for_each_overlapping_leaf_pair(children[0], std::forward<F>(f));
      for_each_overlapping_leaf_pair(children[1], std::forward<F>(f));
    }
  }

  template <typename F>
  static void
  for_each_overlapping_leaf_pair(Node const *root, F &&f) noexcept(noexcept(
    f(std::declval<Payload const &>(), std::declval<Payload const &>()))) {
    assert(root != nullptr);
    if (!root->is_leaf()) {
      auto const &children = root->children;
      for_each_overlapping_leaf_pair(
        children[0], children[1], std::forward<F>(f));
      for_each_overlapping_leaf_pair(children[0], std::forward<F>(f));
      for_each_overlapping_leaf_pair(children[1], std::forward<F>(f));
    }
  }

  template <typename F>
  static void
  for_each_overlapping_leaf_pair(Node *left, Node *right, F &&f) noexcept(
    noexcept(f(std::declval<Payload &>(), std::declval<Payload &>()))) {
    if (left->bounds.intersects(right->bounds)) {
      if (!left->is_leaf()) {
        if (!right->is_leaf()) {
          for (auto const left_child : left->children) {
            for (auto const right_child : right->children) {
              for_each_overlapping_leaf_pair(
                left_child, right_child, std::forward<F>(f));
            }
          }
        } else {
          for (auto const left_child : left->children) {
            for_each_overlapping_leaf_pair(
              left_child, right, std::forward<F>(f));
          }
        }
      } else if (!right->is_leaf()) {
        for (auto const right_child : right->children) {
          for_each_overlapping_leaf_pair(left, right_child, std::forward<F>(f));
        }
      } else {
        f(*left->payload, *right->payload);
      }
    }
  }

  template <typename F>
  static void
  for_each_overlapping_leaf_pair(Node const *left, Node const *right, F &&f) noexcept(
    noexcept(
      f(std::declval<Payload const &>(), std::declval<Payload const &>()))) {
    if (left->bounds.intersects(right->bounds)) {
      if (!left->is_leaf()) {
        if (!right->is_leaf()) {
          for (auto const left_child : left->children) {
            for (auto const right_child : right->children) {
              for_each_overlapping_leaf_pair(
                left_child, right_child, std::forward<F>(f));
            }
          }
        } else {
          for (auto const left_child : left->children) {
            for_each_overlapping_leaf_pair(
              left_child, right, std::forward<F>(f));
          }
        }
      } else if (!right->is_leaf()) {
        for (auto const right_child : right->children) {
          for_each_overlapping_leaf_pair(left, right_child, std::forward<F>(f));
        }
      } else {
        f(*left->payload, *right->payload);
      }
    }
  }

  std::pmr::unsynchronized_pool_resource _pool;
  std::pmr::polymorphic_allocator<Node> _node_allocator;
  std::vector<Node *> _leaf_nodes;
  std::vector<Node *> _internal_nodes;
  std::vector<Node *> _scratch_leaf_nodes;
  Node *_root_node{};
};

} // namespace fpsparty::game

#endif
