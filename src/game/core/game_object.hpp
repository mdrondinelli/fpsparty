#ifndef FPSPARTY_GAME_GAME_OBJECT_HPP
#define FPSPARTY_GAME_GAME_OBJECT_HPP

#include "game/core/game_object_id.hpp"
#include "rc.hpp"
#include <vector>

namespace fpsparty::game {
class Game_object;

namespace detail {
void on_remove_game_object(Game_object &game_object) noexcept;
}

class Game_object_remove_listener {
public:
  virtual ~Game_object_remove_listener() = default;

  virtual void on_remove_game_object() = 0;
};

class Game_object : public rc::Object<Game_object> {
public:
  explicit Game_object(Game_object_id id) noexcept;

  Game_object_id get_game_object_id() const noexcept;

  bool add_remove_listener(Game_object_remove_listener *listener);

  bool remove_remove_listener(Game_object_remove_listener *listener) noexcept;

protected:
  virtual void on_remove() = 0;

private:
  friend void detail::on_remove_game_object(Game_object &game_object) noexcept;

  Game_object_id _game_object_id{};
  std::vector<Game_object_remove_listener *> _removal_listeners{};
};
} // namespace fpsparty::game

#endif
