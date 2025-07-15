#ifndef FPSPARTY_GAME_REPLICATED_WORLD_HPP
#define FPSPARTY_GAME_REPLICATED_WORLD_HPP

#include "game/client/replicated_humanoid.hpp"
#include "game/client/replicated_player.hpp"
#include "game/client/replicated_projectile.hpp"
#include "rc.hpp"
#include "serial/reader.hpp"
#include <exception>
#include <vector>

namespace fpsparty::game {
struct Replicated_world_load_info {
  serial::Reader *public_state_reader{};
  serial::Reader *player_state_reader{};
  std::uint8_t player_state_count{};
};

class Replicated_world_load_error : public std::exception {};

class Replicated_world {
public:
  Replicated_world() = default;

  Replicated_world(const Replicated_world &other) = delete;

  Replicated_world &operator=(const Replicated_world &other) = delete;

  void load(const Replicated_world_load_info &info);

  void reset();

  rc::Strong<Replicated_player>
  get_player_by_entity_id(Entity_id id) const noexcept;

  rc::Strong<Replicated_humanoid>
  get_humanoid_by_entity_id(Entity_id id) const noexcept;

  rc::Strong<Replicated_projectile>
  get_projectile_by_entity_id(Entity_id id) const noexcept;

  std::pmr::vector<rc::Strong<Replicated_player>>
  get_players(std::pmr::memory_resource *memory_resource =
                  std::pmr::get_default_resource()) const;

  std::pmr::vector<rc::Strong<Replicated_humanoid>>
  get_humanoids(std::pmr::memory_resource *memory_resource =
                    std::pmr::get_default_resource()) const;

  std::pmr::vector<rc::Strong<Replicated_projectile>>
  get_projectiles(std::pmr::memory_resource *memory_resource =
                      std::pmr::get_default_resource()) const;

  std::size_t get_player_count() const noexcept;

  std::size_t get_humanoid_count() const noexcept;

  std::size_t get_projectile_count() const noexcept;

private:
  rc::Factory<Replicated_player> _player_factory{};
  rc::Factory<Replicated_humanoid> _humanoid_factory{};
  rc::Factory<Replicated_projectile> _projectile_factory{};
  std::vector<rc::Strong<Replicated_player>> _players{};
  std::vector<rc::Strong<Replicated_humanoid>> _humanoids{};
  std::vector<rc::Strong<Replicated_projectile>> _projectiles{};
};
} // namespace fpsparty::game

#endif
