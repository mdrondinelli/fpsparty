#ifndef FPSPARTY_GAME_GAME_OBJECT_HPP
#define FPSPARTY_GAME_GAME_OBJECT_HPP

#include "rc.hpp"
#include <vector>

namespace fpsparty::game {
class Game_object_remove_listener {
public:
  virtual ~Game_object_remove_listener() = default;

  virtual void on_remove_game_object() = 0;
};

class Game_object : public rc::Object<Game_object> {
public:
  virtual void on_remove() = 0;

  bool add_remove_listener(Game_object_remove_listener *listener);

  bool remove_remove_listener(Game_object_remove_listener *listener) noexcept;

private:
  friend class World;

  std::vector<Game_object_remove_listener *> _removal_listeners{};
};
} // namespace fpsparty::game

#endif
