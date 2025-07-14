#ifndef FPSPARTY_GAME_REPLICATED_PLAYER_HPP
#define FPSPARTY_GAME_REPLICATED_PLAYER_HPP

#include "game/client/replicated_humanoid.hpp"
#include "game/core/game_object.hpp"
#include "game/core/game_object_id.hpp"
#include "game/core/humanoid_input_state.hpp"
#include "game/core/sequence_number.hpp"
#include "rc.hpp"

namespace fpsparty::game {
class Replicated_player : public Game_object,
                          public rc::Object<Replicated_player> {
public:
  explicit Replicated_player(Game_object_id game_object_id);

protected:
  void on_remove() override;

public:
  const rc::Weak<Replicated_humanoid> &get_humanoid() const noexcept;

  void set_humanoid(const rc::Weak<Replicated_humanoid> &value) noexcept;

  const Humanoid_input_state &get_input_state() const noexcept;

  void set_input_state(const Humanoid_input_state &value) noexcept;

  const std::optional<Sequence_number> &
  get_input_sequence_number() const noexcept;

  void set_input_sequence_number(
      const std::optional<Sequence_number> &value) noexcept;

private:
  class Humanoid_remove_listener : public Game_object_remove_listener {
  public:
    explicit Humanoid_remove_listener(Replicated_player *player) noexcept;

    void on_remove_game_object() override;

  private:
    Replicated_player *_player{};
  };

  rc::Weak<Replicated_humanoid> _humanoid{};
  Humanoid_remove_listener _humanoid_remove_listener;
  Humanoid_input_state _input_state{};
  std::optional<Sequence_number> _input_sequence_number{};
};
} // namespace fpsparty::game

#endif
