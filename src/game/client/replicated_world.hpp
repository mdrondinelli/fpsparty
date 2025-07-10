#ifndef FPSPARTY_GAME_REPLICATED_WORLD_HPP
#define FPSPARTY_GAME_REPLICATED_WORLD_HPP

#include "rc.hpp"
#include "serial/reader.hpp"
#include <vector>

namespace fpsparty::game {
struct Replicated_world_load_info {
  serial::Reader *world_state_reader{};
  serial::Reader *player_states_reader{};
  std::uint8_t player_state_count{};
};

class Replicated_world {
public:
  void load(const Replicated_world_load_info &info);

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
