#ifndef FPSPARTY_GAME_REPLICATED_PLAYER_HPP
#define FPSPARTY_GAME_REPLICATED_PLAYER_HPP

#include "game/client/replicated_humanoid.hpp"
#include "game/core/entity.hpp"
#include "game/core/entity_world.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include "net/core/sequence_number.hpp"
#include <memory_resource>

namespace fpsparty::game {
class Replicated_player : public Entity {
public:
  explicit Replicated_player(net::Entity_id entity_id);

protected:
  void on_remove() override;

public:
  Replicated_humanoid *get_humanoid() const noexcept;

  void set_humanoid(Replicated_humanoid *value);

  const net::Input_state &get_input_state() const noexcept;

  void set_input_state(const net::Input_state &value) noexcept;

  const std::optional<net::Sequence_number> &
  get_input_sequence_number() const noexcept;

  void set_input_sequence_number(
      const std::optional<net::Sequence_number> &value) noexcept;

private:
  class Humanoid_remove_listener : public Entity_remove_listener {
  public:
    explicit Humanoid_remove_listener(Replicated_player *player) noexcept;

    void on_remove_entity() override;

  private:
    Replicated_player *_player{};
  };

  Replicated_humanoid *_humanoid{};
  Humanoid_remove_listener _humanoid_remove_listener;
  net::Input_state _input_state{};
  std::optional<net::Sequence_number> _input_sequence_number{};
};

class Replicated_player_load_error : public Entity_world_load_error {};

class Replicated_player_loader : public Entity_loader {
public:
  explicit Replicated_player_loader(
      std::pmr::memory_resource *memory_resource =
          std::pmr::get_default_resource()) noexcept;

  Entity_owner<Entity> create_entity(net::Entity_id entity_id) override;

  void load_entity(serial::Reader &reader, Entity &entity,
                   const Entity_world &world) const override;

  Entity_type get_entity_type() const noexcept override;

private:
  Entity_factory<Replicated_player> _factory{};
};
} // namespace fpsparty::game

#endif
