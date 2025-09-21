#ifndef FPSPARTY_GAME_PLAYER_HPP
#define FPSPARTY_GAME_PLAYER_HPP

#include "game/core/entity.hpp"
#include "game/core/entity_world.hpp"
#include "game/server/humanoid.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include "net/core/sequence_number.hpp"
#include <optional>

namespace fpsparty::game {
struct Player_create_info {};

class Player : public Entity {
public:
  explicit Player(
    net::Entity_id entity_id, const Player_create_info &info) noexcept;

  void on_remove() override;

  Humanoid *get_humanoid() const noexcept;

  void set_humanoid(Humanoid *value) noexcept;

  const net::Input_state &get_input_state() const noexcept;

  std::optional<net::Sequence_number>
  get_input_sequence_number() const noexcept;

  void set_input_state(
    const net::Input_state &input_state,
    net::Sequence_number input_sequence_number) noexcept;

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
  std::optional<net::Sequence_number> _input_sequence_number{};
};

class Player_dumper : public Entity_dumper {
public:
  Entity_type get_entity_type() const noexcept override;

  void dump_entity(serial::Writer &writer, const Entity &entity) const override;
};
} // namespace fpsparty::game

#endif
