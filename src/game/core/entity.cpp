#include "entity.hpp"
#include "game/core/entity_id.hpp"
#include <algorithm>

namespace fpsparty::game {
namespace detail {
void on_remove_entity(Entity &entity) noexcept {
  for (const auto listener : entity._removal_listeners) {
    listener->on_remove_entity();
  }
  entity.on_remove();
}
} // namespace detail

Entity::Entity(Entity_id id) noexcept : _entity_id{id} {}

Entity_id Entity::get_entity_id() const noexcept {
  return _entity_id;
}

bool Entity::add_remove_listener(Entity_remove_listener *listener) {
  const auto it = std::ranges::find(_removal_listeners, listener);
  if (it == _removal_listeners.end()) {
    _removal_listeners.emplace_back(listener);
    return true;
  } else {
    return false;
  }
}

bool Entity::remove_remove_listener(
    Entity_remove_listener *listener) noexcept {
  const auto it = std::ranges::find(_removal_listeners, listener);
  if (it != _removal_listeners.end()) {
    _removal_listeners.erase(it);
    return true;
  } else {
    return false;
  }
}
} // namespace fpsparty::game
