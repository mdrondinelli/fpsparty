#ifndef FPSPARTY_GAME_WORLD_HPP
#define FPSPARTY_GAME_WORLD_HPP

#include "game/server/humanoid.hpp"
#include "game/server/player.hpp"
#include "game/server/projectile.hpp"
#include "rc.hpp"
#include <memory_resource>
#include <vector>

namespace fpsparty::game {
class World {
public:
  void dump(serial::Writer &writer) const;

  bool add(const rc::Strong<Player> &player);

  bool remove(const rc::Strong<Player> &player) noexcept;

  bool add(const rc::Strong<Humanoid> &humanoid);

  bool remove(const rc::Strong<Humanoid> &humanoid) noexcept;

  bool add(const rc::Strong<Projectile> &projectile);

  bool remove(const rc::Strong<Projectile> &projectile) noexcept;

  std::pmr::vector<rc::Strong<Player>>
  get_players(std::pmr::memory_resource *memory_resource =
                  std::pmr::get_default_resource()) const;

  std::pmr::vector<rc::Strong<Humanoid>>
  get_humanoids(std::pmr::memory_resource *memory_resource =
                    std::pmr::get_default_resource()) const;

  std::pmr::vector<rc::Strong<Projectile>>
  get_projectiles(std::pmr::memory_resource *memory_resource =
                      std::pmr::get_default_resource()) const;

  std::size_t get_player_count() const noexcept;

  std::size_t get_humanoid_count() const noexcept;

  std::size_t get_projectile_count() const noexcept;

private:
  void on_remove(Game_object &game_object);

  std::vector<rc::Strong<Player>> _players{};
  std::vector<rc::Strong<Humanoid>> _humanoids{};
  std::vector<rc::Strong<Projectile>> _projectiles{};
};
} // namespace fpsparty::game

#endif
