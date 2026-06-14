#ifndef FPSPARTY_GAME_GRID_HPP
#define FPSPARTY_GAME_GRID_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <math/box.hpp>
#include <math/vec.hpp>
#include <serial/reader.hpp>
#include <serial/writer.hpp>

namespace fpsparty::game {

namespace detail {

constexpr std::size_t linearize_chunk_offset(
  std::array<std::size_t, 3> chunk_counts,
  std::array<std::size_t, 3> chunk_offset) noexcept {
  return chunk_offset[0] + chunk_offset[1] * chunk_counts[0] +
         chunk_offset[2] * chunk_counts[0] * chunk_counts[1];
}

} // namespace detail

enum class Axis { x, y, z };

struct Grid_create_info {
  math::ibox3 bounds{};
};

struct Grid_raycast_hit {
  Eigen::Vector3i cell_indices{};
  Eigen::Vector3i normal{};
  float t{};
};

class Grid_loading_error : public std::exception {};

class Chunk {
public:
  static constexpr auto edge_length = std::size_t{4};

  static constexpr auto get_bit_index(Eigen::Vector3i const &offset) noexcept {
    auto const i = offset.x() % edge_length;
    auto const j = offset.y() % edge_length;
    auto const k = offset.z() % edge_length;
    return k * edge_length * edge_length + j * edge_length + i;
  }

  Chunk() noexcept = default;

  constexpr explicit Chunk(std::uint64_t blocks) noexcept : blocks{blocks} {}

  constexpr bool is_solid(Eigen::Vector3i const &offset) const noexcept {
    auto const bit_index = get_bit_index(offset);
    return blocks & (std::uint64_t{1} << bit_index);
  }

  constexpr void set_solid(Eigen::Vector3i const &offset, bool value) noexcept {
    auto const bit_index = get_bit_index(offset);
    if (value) {
      blocks |= (std::uint64_t{1} << bit_index);
    } else {
      blocks &= ~(std::uint64_t{1} << bit_index);
    }
  }

  std::uint64_t blocks{};

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
    std::pair<Eigen::Vector3i, U *> operator*() const noexcept {
      return {
        {
          static_cast<int>(_chunk_offset[0]),
          static_cast<int>(_chunk_offset[1]),
          static_cast<int>(_chunk_offset[2]),
        },
        _data + detail::linearize_chunk_offset(_chunk_counts, _chunk_offset),
      };
    }

    Iterator_template &operator++() noexcept {
      ++_chunk_offset[0];
      if (_chunk_offset[0] == _chunk_counts[0]) {
        _chunk_offset[0] = 0;
        ++_chunk_offset[1];
        if (_chunk_offset[1] == _chunk_counts[1]) {
          _chunk_offset[1] = 0;
          ++_chunk_offset[2];
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
      return lhs._chunk_offset == rhs._chunk_offset;
    }

  private:
    constexpr Iterator_template(
      U *data,
      std::array<std::size_t, 3> const &chunk_counts,
      std::array<std::size_t, 3> const &chunk_offset) noexcept
        : _data{data},
          _chunk_counts{chunk_counts},
          _chunk_offset{chunk_offset} {}

    U *_data;
    std::array<std::size_t, 3> _chunk_counts;
    std::array<std::size_t, 3> _chunk_offset;
  };

public:
  using Iterator = Iterator_template<T>;
  using Const_iterator = Iterator_template<T const>;

  Chunk_span_template(
    T *data, std::array<std::size_t, 3> const &chunk_counts) noexcept
      : _data{data}, _chunk_counts{chunk_counts} {}

  Iterator begin() const noexcept {
    return {
      _data,
      _chunk_counts,
      {
        std::size_t{},
        std::size_t{},
        std::size_t{},
      }};
  }

  Const_iterator cbegin() const noexcept {
    return {
      _data,
      _chunk_counts,
      {
        std::size_t{},
        std::size_t{},
        std::size_t{},
      }};
  }

  Iterator end() const noexcept {
    return {_data, _chunk_counts, {0, 0, _chunk_counts[2]}};
  }

  Const_iterator cend() const noexcept {
    return {_data, _chunk_counts, {0, 0, _chunk_counts[2]}};
  }

private:
  T *_data;
  std::array<std::size_t, 3> _chunk_counts;
};

using Chunk_span = Chunk_span_template<Chunk>;
using Const_chunk_span = Chunk_span_template<Chunk const>;

class Grid {
public:
  static bool diff(Grid const &lhs, Grid const &rhs);

  explicit Grid(Grid_create_info const &create_info);

  void load(serial::Reader &reader);

  void dump(serial::Writer &writer) const;

  void fill(Eigen::AlignedBox3i const &bounds, bool solid = true);

  void set_solid(Eigen::Vector3i const &cell_coords, bool solid) noexcept;

  bool is_solid(Eigen::Vector3i const &cell_coords) const noexcept;

  std::optional<Grid_raycast_hit> raycast(
    Eigen::Vector3i const &origin_cell_indices,
    Eigen::Vector3f const &origin_cell_offset,
    Eigen::Vector3f const &ray_direction,
    float max_t) const noexcept;

  bool bounds_check_cell(Eigen::Vector3i const &cell_coords) const noexcept;

  Chunk_span get_chunks() noexcept;

  Const_chunk_span get_chunks() const noexcept;

  Const_chunk_span get_const_chunks() const noexcept;

  math::ivec3 get_chunk_counts() const noexcept;

  math::ivec3 get_cell_counts() const noexcept;

  math::ibox3 const &get_bounds() const noexcept;

private:
  math::ibox3 _bounds;
  std::vector<Chunk> _chunks{};
};

} // namespace fpsparty::game

#endif
