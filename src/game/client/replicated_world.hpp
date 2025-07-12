#ifndef FPSPARTY_GAME_REPLICATED_WORLD_HPP
#define FPSPARTY_GAME_REPLICATED_WORLD_HPP

#include "game/client/replicated_humanoid.hpp"
#include "game/client/replicated_player.hpp"
#include "game/client/replicated_projectile.hpp"
#include "rc.hpp"
#include <vector>

namespace fpsparty::game {
class Replicated_world_load_error {};

class Replicated_world {
public:
  void load(serial::Reader &reader);

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
  std::vector<rc::Strong<Replicated_player>> _players{};
  std::vector<rc::Strong<Replicated_humanoid>> _humanoids{};
  std::vector<rc::Strong<Replicated_projectile>> _projectiles{};
};
} // namespace fpsparty::game

#endif
