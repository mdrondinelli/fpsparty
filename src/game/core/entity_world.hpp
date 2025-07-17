#ifndef FPSPARTY_GAME_WORLD_HPP
#define FPSPARTY_GAME_WORLD_HPP

#include "game/core/entity.hpp"
#include "rc.hpp"
#include "serial/writer.hpp"
#include <concepts>
#include <memory_resource>
#include <vector>

namespace fpsparty::game {
class Entity_world {
public:
  void dump(serial::Writer &writer) const;

  bool add(const rc::Strong<Entity> &entity);

  bool remove(const rc::Strong<Entity> &entity) noexcept;

  template <std::derived_from<Entity> T>
  std::pmr::vector<rc::Strong<T>>
  get_entities_of_type(std::pmr::memory_resource *memory_resource =
                           std::pmr::get_default_resource()) const {
    auto retval = std::pmr::vector<rc::Strong<T>>{memory_resource};
    retval.reserve(count_entities_of_type<T>());
    for (const auto &entity : _entities) {
      if (auto typed_entity = entity.downcast<T>()) {
        retval.emplace_back(std::move(typed_entity));
      }
    }
    return retval;
  }

  template <std::derived_from<Entity> T>
  std::size_t count_entities_of_type() const noexcept {
    auto retval = std::size_t{};
    for (const auto &entity : _entities) {
      if (entity.downcast<T>()) {
        ++retval;
      }
    }
    return retval;
  }

private:
  std::vector<rc::Strong<Entity>> _entities{};
};
} // namespace fpsparty::game

#endif
