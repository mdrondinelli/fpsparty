#ifndef FPSPARTY_GAME_GRID_HPP
#define FPSPARTY_GAME_GRID_HPP

#include <cstddef>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <math/box.hpp>
#include <math/vec.hpp>
#include <serial/reader.hpp>
#include <serial/writer.hpp>

#include "block.hpp"

namespace fpsparty::game {

namespace detail {

constexpr std::size_t linearize_chunk_index(
  math::ivec3 chunk_counts, math::ivec3 chunk_indices) noexcept {
  return chunk_indices[0] + chunk_indices[1] * chunk_counts[0] +
         chunk_indices[2] * chunk_counts[0] * chunk_counts[1];
}

} // namespace detail

struct Grid_create_info {
  math::ibox3 bounds{};
};

struct Grid_raycast_hit {
  math::ivec3 cell_coords{};
  math::ivec3 normal{};
  float t{};
};

struct Grid_contact {
  math::ivec3 cell_coords{};
  math::ivec3 normal{};
  float separation{};
};

class Grid_loading_error : public std::exception {};

class Chunk {
public:
  static constexpr auto edge_length = std::size_t{4};

  static constexpr auto get_block_index(math::ivec3 offset) noexcept {
    auto const i = offset.x() & (edge_length - 1);
    auto const j = offset.y() & (edge_length - 1);
    auto const k = offset.z() & (edge_length - 1);
    return k * edge_length * edge_length + j * edge_length + i;
  }

  constexpr Chunk() noexcept = default;

  constexpr std::pair<Block, int> get_block(math::ivec3 offset) const noexcept {
    auto const idx = get_block_index(offset);
    return unpack_block_data(bytes[idx]);
  }

  constexpr void set_block(math::ivec3 offset, Block value, int data) noexcept {
    auto const idx = get_block_index(offset);
    bytes[idx] = pack_block_data(value, data);
  }

  constexpr bool is_solid(math::ivec3 offset) const noexcept {
    auto const idx = get_block_index(offset);
    return static_cast<bool>(bytes[idx]);
  }

  std::array<std::byte, 64> bytes{};

  friend constexpr bool
  operator==(Chunk const &lhs, Chunk const &rhs) noexcept = default;
};

template <typename T>
  requires std::is_same_v<std::remove_const_t<T>, Chunk>
class Chunk_span_template {
  template <typename U>
    requires std::is_same_v<std::remove_const_t<U>, Chunk>
  class Iterator_template {
    friend class Chunk_span_template;

  public:
    std::pair<math::ivec3, U *> operator*() const noexcept {
      return {
        _chunk_coords,
        _data + detail::linearize_chunk_index(
                  _chunk_bounds.diagonal() + math::ivec3::Ones(),
                  _chunk_coords - _chunk_bounds.min()),
      };
    }

    Iterator_template &operator++() noexcept {
      ++_chunk_coords[0];
      if (_chunk_coords[0] > _chunk_bounds.max()[0]) {
        _chunk_coords[0] = _chunk_bounds.min()[0];
        ++_chunk_coords[1];
        if (_chunk_coords[1] > _chunk_bounds.max()[1]) {
          _chunk_coords[1] = _chunk_bounds.min()[1];
          ++_chunk_coords[2];
        }
      }
      return *this;
    }

    Iterator_template operator++(int) noexcept {
      auto temp{*this};
      ++*this;
      return temp;
    }

    friend bool operator==(
      Iterator_template const &lhs, Iterator_template const &rhs) noexcept {
      return lhs._chunk_coords == rhs._chunk_coords;
    }

  private:
    constexpr Iterator_template(
      U *data,
      math::ibox3 const &chunk_bounds,
      math::ivec3 chunk_coords) noexcept
        : _data{data},
          _chunk_bounds{chunk_bounds},
          _chunk_coords{chunk_coords} {}

    U *_data;
    math::ibox3 _chunk_bounds;
    math::ivec3 _chunk_coords;
  };

public:
  using Iterator = Iterator_template<T>;
  using Const_iterator = Iterator_template<T const>;

  Chunk_span_template(T *data, math::ibox3 const &chunk_bounds) noexcept
      : _data{data}, _chunk_bounds{chunk_bounds} {}

  Iterator begin() const noexcept {
    if (!_chunk_bounds.isEmpty()) {
      return {_data, _chunk_bounds, _chunk_bounds.min()};
    } else {
      return end();
    }
  }

  Const_iterator cbegin() const noexcept {
    if (!_chunk_bounds.isEmpty()) {
      return {_data, _chunk_bounds, _chunk_bounds.min()};
    } else {
      return end();
    }
  }

  Iterator end() const noexcept {
    return {
      _data,
      _chunk_bounds,
      {_chunk_bounds.min()[0],
       _chunk_bounds.min()[1],
       _chunk_bounds.max()[2] + 1}};
  }

  Const_iterator cend() const noexcept {
    return {
      _data,
      _chunk_bounds,
      {_chunk_bounds.min()[0],
       _chunk_bounds.min()[1],
       _chunk_bounds.max()[2] + 1}};
  }

private:
  T *_data;
  math::ibox3 _chunk_bounds;
};

using Chunk_span = Chunk_span_template<Chunk>;
using Const_chunk_span = Chunk_span_template<Chunk const>;

class Grid {
public:
  static bool diff(Grid const &lhs, Grid const &rhs);

  explicit Grid(Grid_create_info const &create_info);

  void load(serial::Reader &reader);

  void dump(serial::Writer &writer) const;

  void fill(math::ibox3 const &cells, Block block, int data = 0);

  std::optional<Grid_raycast_hit> raycast(
    math::ivec3 origin_cell_coords,
    math::vec3 origin_cell_offset,
    math::vec3 ray_direction,
    float max_t) const noexcept;

  std::optional<Grid_contact>
  find_contact(math::box3 const &box) const noexcept;

  // returns true iff cell_coords was in bounds
  bool set_block(math::ivec3 cell_coords, Block block, int data = 0) noexcept;

  std::pair<Block, int> get_block(math::ivec3 cell_coords) const noexcept;

  bool is_solid(math::ivec3 cell_coords) const noexcept;

  bool bounds_check_cell(math::ivec3 coords) const noexcept;

private:
  Chunk *get_chunk_unsafe(math::ivec3 coords) noexcept;

  Chunk const *get_chunk_unsafe(math::ivec3 coords) const noexcept;

  std::size_t linearize_chunk_index(math::ivec3 indices) const noexcept;

public:
  Chunk_span get_chunks() noexcept;

  Const_chunk_span get_chunks() const noexcept;

  Const_chunk_span get_const_chunks() const noexcept;

  math::ivec3 get_chunk_counts() const noexcept;

  math::ivec3 get_cell_counts() const noexcept;

  math::ibox3 get_chunk_bounds() const noexcept;

  math::ibox3 const &get_cell_bounds() const noexcept;

  bool empty() const noexcept;

  static math::ibox3 world_to_cell(math::box3 const &coords);

  static math::ibox3 cell_to_chunk(math::ibox3 const &coords);

  static math::ivec3 cell_to_chunk(math::ivec3 coords);

  static math::ivec3 chunk_to_cell(math::ivec3 coords);

private:
  math::ibox3 _cell_bounds;
  std::vector<Chunk> _chunks{};
};

} // namespace fpsparty::game

#endif
