#ifndef FPSPARTY_GAME_PLAYER_HPP
#define FPSPARTY_GAME_PLAYER_HPP

#include "game/entity.hpp"
#include "game/entity_world.hpp"
#include "game/humanoid.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"

namespace fpsparty::game {
struct Player_create_info {};

class Player : public Entity {
public:
  explicit Player(
    net::Entity_id entity_id, Player_create_info const &info) noexcept;

  void on_remove() override;

  Humanoid *get_humanoid() const noexcept;

  void set_humanoid(Humanoid *value) noexcept;

  net::Input_state const &get_input_state() const noexcept;

  void set_input_state(net::Input_state const &input_state) noexcept;

private:
  class Humanoid_remove_listener : public Entity_remove_listener {
  public:
    explicit Humanoid_remove_listener(Player *player) noexcept;

    void on_remove_entity() override;

  private:
    Player *_player{};
  };

  Humanoid *_humanoid{};
  Humanoid_remove_listener _humanoid_remove_listener;
  net::Input_state _input_state{};
};

class Player_dumper : public Entity_dumper {
public:
  Entity_type get_entity_type() const noexcept override;

  void dump_entity(serial::Writer &writer, Entity const &entity) const override;
};
} // namespace fpsparty::game

#endif
