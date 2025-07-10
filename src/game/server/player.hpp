#ifndef FPSPARTY_GAME_PLAYER_HPP
#define FPSPARTY_GAME_PLAYER_HPP

#include "game/core/game_object.hpp"
#include "game/core/humanoid_input_state.hpp"
#include "game/server/humanoid.hpp"
#include "rc.hpp"
#include <optional>

namespace fpsparty::game {
struct Player_create_info {};

class Player : public Game_object, rc::Object<Player> {
public:
  explicit Player(std::uint32_t network_id,
                  const Player_create_info &info) noexcept;

  void on_remove() override;

  void dump(serial::Writer &writer) const;

  std::uint32_t get_network_id() const noexcept;

  const rc::Weak<Humanoid> &get_humanoid() const noexcept;

  void set_humanoid(const rc::Weak<Humanoid> &value) noexcept;

  const Humanoid_input_state &get_input_state() const noexcept;

  std::optional<std::uint16_t> get_input_sequence_number() const noexcept;

  void set_input_state(const Humanoid_input_state &input_state,
                       std::uint16_t input_sequence_number) noexcept;

private:
  struct Humanoid_remove_listener : Game_object_remove_listener {
    Player *player;

    explicit Humanoid_remove_listener(Player *player) noexcept;

    void on_remove_game_object() override;
  };

  std::uint32_t _network_id{};
  rc::Weak<Humanoid> _humanoid{};
  Humanoid_remove_listener _humanoid_remove_listener;
  Humanoid_input_state _input_state{};
  std::optional<std::uint16_t> _input_sequence_number{};
};
} // namespace fpsparty::game

#endif
