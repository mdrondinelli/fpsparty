#ifndef FPSPARTY_GAME_ENTITY_HPP
#define FPSPARTY_GAME_ENTITY_HPP

#include "game/core/entity_id.hpp"
#include "rc.hpp"
#include <vector>

namespace fpsparty::game {
class Entity;

namespace detail {
void on_remove_entity(Entity &entity) noexcept;
}

class Entity_remove_listener {
public:
  virtual ~Entity_remove_listener() = default;

  virtual void on_remove_entity() = 0;
};

class Entity : public rc::Object<Entity> {
public:
  explicit Entity(Entity_id id) noexcept;

  Entity_id get_entity_id() const noexcept;

  bool add_remove_listener(Entity_remove_listener *listener);

  bool remove_remove_listener(Entity_remove_listener *listener) noexcept;

protected:
  virtual void on_remove() = 0;

private:
  friend void detail::on_remove_entity(Entity &entity) noexcept;

  Entity_id _entity_id{};
  std::vector<Entity_remove_listener *> _removal_listeners{};
};
} // namespace fpsparty::game

#endif
