#ifndef FPSPARTY_VMA_HPP
#define FPSPARTY_VMA_HPP

#include <exception>
#include <utility>
#include <volk.h>
#include <vulkan/vulkan.hpp>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-private-field"
#include <vk_mem_alloc.h>
#pragma clang diagnostic pop

namespace fpsparty::vma {
class Allocator {
public:
  using Create_info = VmaAllocatorCreateInfo;

  class Creation_error : public std::exception {};

  constexpr Allocator() noexcept = default;

  constexpr Allocator(VmaAllocator value) noexcept : _value{value} {}

  constexpr operator VmaAllocator() const noexcept { return _value; }

private:
  VmaAllocator _value{};
};

inline Allocator create_allocator(const Allocator::Create_info &create_info) {
  auto value = VmaAllocator{};
  const auto result = vmaCreateAllocator(&create_info, &value);
  if (vk::Result{result} != vk::Result::eSuccess) {
    throw Allocator::Creation_error{};
  }
  return value;
}

inline void destroy_allocator(Allocator allocator) {
  vmaDestroyAllocator(allocator);
}

class Unique_allocator {
public:
  constexpr Unique_allocator() noexcept = default;

  constexpr explicit Unique_allocator(Allocator value) noexcept
      : _value{value} {}

  constexpr Unique_allocator(Unique_allocator &&other) noexcept
      : _value{std::exchange(other._value, nullptr)} {}

  Unique_allocator &operator=(Unique_allocator &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_allocator() { destroy_allocator(_value); }

  constexpr operator bool() const noexcept { return _value; }

private:
  constexpr void swap(Unique_allocator &other) noexcept {
    std::swap(_value, other._value);
  }

  Allocator _value{};
};

inline Unique_allocator
create_allocator_unique(const Allocator::Create_info &create_info) {
  return Unique_allocator{create_allocator(create_info)};
}

using Vulkan_functions = VmaVulkanFunctions;

class Function_importing_error : public std::exception {};

inline Vulkan_functions
import_functions_from_volk(const Allocator::Create_info &create_info) {
  auto vulkan_functions = VmaVulkanFunctions{};
  const auto result =
      vmaImportVulkanFunctionsFromVolk(&create_info, &vulkan_functions);
  if (vk::Result{result} != vk::Result::eSuccess) {
    throw Function_importing_error{};
  }
  return vulkan_functions;
}
} // namespace fpsparty::vma

#endif
