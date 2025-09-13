#ifndef FPSPARTY_FLAGS_HPP
#define FPSPARTY_FLAGS_HPP

#include <type_traits>

namespace fpsparty {
template <class T>
  requires(std::is_enum_v<T>)
class Flags {
public:
  using Value = std::underlying_type_t<T>;

  constexpr Flags() noexcept = default;

  constexpr Flags(T value) noexcept : _value{static_cast<Value>(value)} {}

  constexpr explicit Flags(Value value) noexcept : _value{value} {}

  constexpr operator Value() const noexcept { return _value; }

  friend constexpr Flags operator|(Flags lhs, Flags rhs) noexcept {
    return Flags{lhs._value | rhs._value};
  }

  friend constexpr Flags &operator|=(Flags &lhs, Flags rhs) noexcept {
    return lhs = lhs | rhs;
  }

private:
  Value _value{};
};
} // namespace fpsparty

#endif
