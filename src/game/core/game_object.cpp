#include "game_object.hpp"
#include <algorithm>

namespace fpsparty::game {
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
