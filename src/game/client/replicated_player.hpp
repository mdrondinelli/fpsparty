#ifndef FPSPARTY_GAME_REPLICATED_PLAYER_HPP
#define FPSPARTY_GAME_REPLICATED_PLAYER_HPP

#include "game/client/replicated_humanoid.hpp"
#include "game/core/entity.hpp"
#include "game/core/entity_id.hpp"
#include "game/core/entity_world.hpp"
#include "game/core/humanoid_input_state.hpp"
#include "game/core/sequence_number.hpp"
#include "rc.hpp"
#include <memory_resource>

namespace fpsparty::game {
class Replicated_player : public Entity, public rc::Object<Replicated_player> {
public:
  explicit Replicated_player(Entity_id entity_id);

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
  class Humanoid_remove_listener : public Entity_remove_listener {
  public:
    explicit Humanoid_remove_listener(Replicated_player *player) noexcept;

    void on_remove_entity() override;

  private:
    Replicated_player *_player{};
  };

  rc::Weak<Replicated_humanoid> _humanoid{};
  Humanoid_remove_listener _humanoid_remove_listener;
  Humanoid_input_state _input_state{};
  std::optional<Sequence_number> _input_sequence_number{};
};

class Replicated_player_load_error : public Entity_world_load_error {};

class Replicated_player_loader : public Entity_loader {
public:
  explicit Replicated_player_loader(
      std::pmr::memory_resource *memory_resource =
          std::pmr::get_default_resource()) noexcept;

  rc::Strong<Entity> create_entity(Entity_id entity_id) override;

  void load_entity(serial::Reader &reader, Entity &entity,
                   const Entity_world &world) const override;

  Entity_type get_entity_type() const noexcept override;

private:
  rc::Factory<Replicated_player> _factory{};
};
} // namespace fpsparty::game

#endif
