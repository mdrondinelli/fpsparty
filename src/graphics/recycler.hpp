#ifndef FPSPARTY_GRAPHICS_RECYCLER_HPP
#define FPSPARTY_GRAPHICS_RECYCLER_HPP

#include "algorithms/unordered_erase.hpp"
#include "graphics/image.hpp"
#include "graphics/work.hpp"
#include "graphics/work_done_callback.hpp"
#include "rc.hpp"
#include <algorithm>
#include <utility>
#include <vector>

namespace fpsparty::graphics {
template <class Resource, class Predicate, class Factory>
class Recycler : Work_done_callback {
public:
  Recycler() = default;

  template <typename P, typename F>
  explicit Recycler(P &&predicate, F &&factory)
      : _predicate{std::forward<P>(predicate)},
        _factory{std::forward<F>(factory)} {}

  Recycler(const Recycler &other) = delete;

  Recycler &operator=(const Recycler &other) = delete;

  void push(rc::Strong<Resource> resource, rc::Strong<Work> work) {
    if (_predicate(*resource)) {
      _unfree_resources.emplace_back(std::move(resource), work);
      work->add_done_callback(this);
    }
  }

  rc::Strong<Resource> pop(const Predicate &predicate) {
    set_predicate(predicate);
    return pop();
  }

  rc::Strong<Resource> pop(Predicate &&predicate) {
    set_predicate(std::move(predicate));
    return pop();
  }

  rc::Strong<Resource> pop() {
    if (!_free_resources.empty()) {
      auto retval = std::move(_free_resources.back());
      _free_resources.pop_back();
      return retval;
    } else {
      return _factory(_predicate);
    }
  }

  const Predicate &get_predicate() const noexcept { return _predicate; }

  void set_predicate(const Predicate &predicate) noexcept {
    _predicate = predicate;
    on_change_predicate();
  }

  void set_predicate(Predicate &&predicate) noexcept {
    _predicate = std::move(predicate);
    on_change_predicate();
  }

  const Factory &get_factory() const noexcept { return _factory; }

  void set_factory(const Factory &factory) noexcept { _factory = factory; }

  void set_factory(Factory &&factory) noexcept {
    _factory = std::move(factory);
  }

private:
  void on_change_predicate() {
    algorithms::unordered_erase_many_if(
        _free_resources, [&](const rc::Strong<Resource> &resource) {
          return !_predicate(*resource);
        });
  }

  void on_work_done(const Work &work) {
    const auto it = std::ranges::find_if(
        _unfree_resources,
        [&](const std::pair<rc::Strong<Resource>, rc::Strong<Work>> &pair) {
          return pair.second.get() == &work;
        });
    if (it != _unfree_resources.end()) {
      if (_predicate(*it->first)) {
        _free_resources.emplace_back(std::move(it->first));
      }
      algorithms::unordered_erase_at(_unfree_resources, it);
    }
  }

  std::vector<rc::Strong<Resource>> _free_resources;
  std::vector<std::pair<rc::Strong<Resource>, rc::Strong<Work>>>
      _unfree_resources;
  Predicate _predicate;
  Factory _factory;
};

namespace recycler_predicates {
class Image_extent {
public:
  explicit Image_extent(const Eigen::Vector3i &extent) : _extent{extent} {}

  bool operator()(const Image &image) const noexcept {
    return image.get_extent() == _extent;
  }

  const Eigen::Vector3i &get_extent() const noexcept { return _extent; }

private:
  Eigen::Vector3i _extent;
};
} // namespace recycler_predicates
} // namespace fpsparty::graphics

#endif
