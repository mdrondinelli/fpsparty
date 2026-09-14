#ifndef FPSPARTY_GRAPHICS_DEBUG_NAME_HPP
#define FPSPARTY_GRAPHICS_DEBUG_NAME_HPP

#include "int.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {

// A name for a Vulkan object, shown by graphics debuggers and by
// validation messages. Implicitly convertible from a string literal, so
// call sites read as plain strings and need no #ifdef; empty when names
// are compiled out, so an FPSPARTY_NO_UNIQUE_ADDRESS member of one costs
// nothing.
//
// Holds a pointer rather than a string: names are literals, and Vulkan
// copies the text during the call, so there is no lifetime to manage.
#ifdef FPSPARTY_VULKAN_DEBUG_NAMES
class Debug_name {
public:
  constexpr Debug_name() noexcept = default;

  constexpr Debug_name(char const *name) noexcept : _name{name} {}

  constexpr char const *c_str() const noexcept { return _name; }

private:
  char const *_name{};
};
#else
class Debug_name {
public:
  constexpr Debug_name() noexcept = default;

  constexpr Debug_name(char const *) noexcept {}

  constexpr char const *c_str() const noexcept { return nullptr; }
};
#endif

namespace detail {
// Names one Vulkan object. Does nothing when names are compiled out, or
// when the name is empty.
void set_debug_name(
  vk::ObjectType object_type, u64 object_handle, Debug_name name) noexcept;
} // namespace detail

} // namespace fpsparty::graphics

#endif
