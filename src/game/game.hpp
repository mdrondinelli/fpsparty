#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include <int.hpp>
#include <flt.hpp>

#include "entity_world.hpp"
#include "grid.hpp"

namespace fpsparty::game {
struct Game_create_info {
  // The bounds of the grid in cell coordinates (inclusive).
  math::ibox3 grid_bounds{};
  
  // Tilt of the planet's axis in radians.
  f32 axial_tilt{0.409105f};

  // Latitude of the origin of world space.
  f32 origin_latitude{0.57098f};

  // Longitude of the origin of world space.
  f32 origin_longitude{-2.044871f};

  // Time in real-time seconds between noons of consecutive in-game days.
  f32 solar_day_duration{20.0f * 60.0f};

  // Time in in-game (synocdic) days between consecutive summer solstices.
  f32 tropical_year_duration{365.24217f};

  // The initial solar time in microseconds since the epoch.
  u64 initial_solar_time{};
};

class Game {
  struct Impl;

public:
  explicit Game(Game_create_info const &info);

  void tick(f32 duration);

  Grid const &get_grid() const noexcept { return _grid; }

  Grid &get_grid() noexcept { return _grid; }

  Entity_world const &get_entities() const noexcept { return _entities; }

  Entity_world &get_entities() noexcept { return _entities; }

  // Returns the sun direction in world-space.
  math::vec3 get_sun_direction() const noexcept;

  // Returns the solar time in microseconds since the epoch.
  //
  // The epoch is the instant of the summer solstice in the northern hemisphere,
  // solar noon at the prime meridian (longitude 0).
  //
  // Since the length of the solar day is constant, solar time is synonymous
  // with atomic time.
  //
  // 1 solar day = 86,400,000,000 microseconds
  u64 get_solar_time() const noexcept {
    return _solar_time;
  }

  f32 get_axial_tilt() const noexcept {
    return _axial_tilt;
  }

  f32 get_origin_latitude() const noexcept {
    return _origin_latitude;
  }

  f32 get_origin_longitude() const noexcept {
    return _origin_longitude;
  }

private:
  Block_bounds_registry _block_bounds_registry;
  Grid _grid;
  Entity_world _entities{};
  f32 _axial_tilt{};
  f32 _origin_latitude{};
  f32 _origin_longitude{};
  f32 _solar_day_duration{};
  f32 _tropical_year_duration{};
  u64 _solar_time{};
};
} // namespace fpsparty::game

#endif
