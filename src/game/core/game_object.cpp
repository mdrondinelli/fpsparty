#include "game_object.hpp"
#include "game/core/game_object_id.hpp"
#include <algorithm>

namespace fpsparty::game {
namespace detail {
void on_remove_game_object(Game_object &game_object) noexcept {
  for (const auto listener : game_object._removal_listeners) {
    listener->on_remove_game_object();
  }
  game_object.on_remove();
}
} // namespace detail

Game_object::Game_object(Game_object_id id) noexcept : _game_object_id{id} {}

Game_object_id Game_object::get_game_object_id() const noexcept {
  return _game_object_id;
}

bool Game_object::add_remove_listener(Game_object_remove_listener *listener) {
  const auto it = std::ranges::find(_removal_listeners, listener);
  if (it == _removal_listeners.end()) {
    _removal_listeners.emplace_back(listener);
    return true;
  } else {
    return false;
  }
}

bool Game_object::remove_remove_listener(
    Game_object_remove_listener *listener) noexcept {
  const auto it = std::ranges::find(_removal_listeners, listener);
  if (it != _removal_listeners.end()) {
    _removal_listeners.erase(it);
    return true;
  } else {
    return false;
  }
}
} // namespace fpsparty::game
