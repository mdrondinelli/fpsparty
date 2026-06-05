#ifndef FPSPARTY_GAME_PLAYER_HPP
#define FPSPARTY_GAME_PLAYER_HPP

#include <net/core/entity_id.hpp>
#include <net/core/input_state.hpp>
#include <serial/writer.hpp>

#include "entity_traits.hpp"
#include "entity_type.hpp"

namespace fpsparty::game {

struct Player {
  net::Entity_id humanoid{};
  net::Input_state input_state{};
};

template <>
struct Entity_traits<Player> {
  static constexpr Entity_type type = Entity_type::player;

  static void dump(serial::Writer &writer, Player const &entity);
};

} // namespace fpsparty::game

#endif
