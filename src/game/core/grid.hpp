#ifndef FPSPARTY_GAME_CORE_GRID_HPP
#define FPSPARTY_GAME_CORE_GRID_HPP

#include "serial/reader.hpp"
#include "serial/writer.hpp"
#include <Eigen/Dense>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

namespace fpsparty::game {
namespace detail {
constexpr std::size_t
linearize_chunk_offset(std::array<std::size_t, 3> chunk_counts,
                       std::array<std::size_t, 3> chunk_offset) noexcept {
  return chunk_offset[0] + chunk_offset[1] * chunk_counts[0] +
         chunk_offset[2] * chunk_counts[0] * chunk_counts[1];
}
} // namespace detail

enum class Axis { x, y, z };

struct Grid_create_info {
  std::size_t width{};
  std::size_t height{};
  std::size_t depth{};
};

class Grid_loading_error : public std::exception {};

class Chunk {
public:
  static constexpr auto edge_length = std::size_t{4};

  static constexpr auto get_bit_index(const Eigen::Vector3i &offset) noexcept {
    const auto i = offset.x() % edge_length;
    const auto j = offset.y() % edge_length;
    const auto k = offset.z() % edge_length;
    return k * edge_length * edge_length + j * edge_length + i;
  }

  Chunk() noexcept = default;

  constexpr Chunk(std::uint64_t x_bits,
                  std::uint64_t y_bits,
                  std::uint64_t z_bits) noexcept
      : bits{x_bits, y_bits, z_bits} {}

  constexpr bool is_solid(Axis axis,
                          const Eigen::Vector3i &offset) const noexcept {
    const auto bit_index = get_bit_index(offset);
    return bits[static_cast<int>(axis)] & (std::uint64_t{1} << bit_index);
  }

  constexpr void
  set_solid(Axis axis, const Eigen::Vector3i &offset, bool value) noexcept {
    const auto bit_index = get_bit_index(offset);
    if (value) {
      bits[static_cast<int>(axis)] |= (std::uint64_t{1} << bit_index);
    } else {
      bits[static_cast<int>(axis)] &= ~(std::uint64_t{1} << bit_index);
    }
  }

  std::array<std::uint64_t, 3> bits;
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

    friend bool operator==(const Iterator_template &lhs,
                           const Iterator_template &rhs) noexcept {
      return lhs._chunk_offset == rhs._chunk_offset;
    }

  private:
    constexpr Iterator_template(
        U *data,
        const std::array<std::size_t, 3> &chunk_counts,
        const std::array<std::size_t, 3> &chunk_offset) noexcept
        : _data{data},
          _chunk_counts{chunk_counts},
          _chunk_offset{chunk_offset} {}

    U *_data;
    std::array<std::size_t, 3> _chunk_counts;
    std::array<std::size_t, 3> _chunk_offset;
  };

public:
  using Iterator = Iterator_template<T>;
  using Const_iterator = Iterator_template<const T>;

  Chunk_span_template(T *data,
                      const std::array<std::size_t, 3> &chunk_counts) noexcept
      : _data{data}, _chunk_counts{chunk_counts} {}

  Iterator begin() const noexcept {
    return {_data,
            _chunk_counts,
            {
                std::size_t{},
                std::size_t{},
                std::size_t{},
            }};
  }

  Const_iterator cbegin() const noexcept {
    return {_data,
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
using Const_chunk_span = Chunk_span_template<const Chunk>;

class Grid {
public:
  explicit Grid(const Grid_create_info &create_info);

  void load(serial::Reader &reader);

  void dump(serial::Writer &writer) const;

  void fill(Axis normal, int layer, const Eigen::AlignedBox2i &bounds);

  Chunk_span get_chunks() noexcept;

  Const_chunk_span get_chunks() const noexcept;

  Const_chunk_span get_const_chunks() const noexcept;

  std::size_t get_width() const noexcept;

  std::size_t get_height() const noexcept;

  std::size_t get_depth() const noexcept;

private:
  std::array<std::size_t, 3> _chunk_counts;
  std::vector<Chunk> _chunks{};
};
} // namespace fpsparty::game

#endif
