#ifndef FPSPARTY_VMA_HPP
#define FPSPARTY_VMA_HPP

#include <cstring>
#include <exception>
#include <tuple>
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
using Allocation_create_info = VmaAllocationCreateInfo;
using Allocation_info = VmaAllocationInfo;

class Buffer_creation_error : std::exception {};

class Allocation;

class Unique_allocation;

class Allocator {
public:
  using Create_info = VmaAllocatorCreateInfo;

  class Creation_error : public std::exception {};

  constexpr Allocator() noexcept = default;

  constexpr Allocator(VmaAllocator value) noexcept : _value{value} {}

  constexpr operator VmaAllocator() const noexcept { return _value; }

  std::tuple<vk::Buffer, vma::Allocation>
  create_buffer(const vk::BufferCreateInfo &buffer_create_info,
                const Allocation_create_info &allocation_create_info,
                Allocation_info *allocation_info = nullptr) const;

  std::tuple<vk::UniqueBuffer, vma::Unique_allocation>
  create_buffer_unique(const vk::BufferCreateInfo &buffer_create_info,
                       const Allocation_create_info &allocation_create_info,
                       Allocation_info *allocation_info = nullptr) const;

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

class Allocation {
public:
  constexpr Allocation() noexcept = default;

  constexpr Allocation(VmaAllocation value) noexcept : _value{value} {}

  constexpr operator VmaAllocation() const noexcept { return _value; }

private:
  VmaAllocation _value{};
};

class Unique_allocation {
public:
  constexpr Unique_allocation() noexcept = default;

  constexpr explicit Unique_allocation(Allocation value,
                                       Allocator owner) noexcept
      : _value{value}, _owner{owner} {}

  constexpr Unique_allocation(Unique_allocation &&other) noexcept
      : _value{std::exchange(other._value, nullptr)}, _owner{std::exchange(
                                                          other._owner,
                                                          nullptr)} {}

  Unique_allocation &operator=(Unique_allocation &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_allocation() {
    if (_owner) {
      vmaFreeMemory(_owner, _value);
    }
  }

  constexpr operator bool() const noexcept { return _value != nullptr; }

private:
  constexpr void swap(Unique_allocation &other) noexcept {
    std::swap(_value, other._value);
    std::swap(_owner, other._owner);
  }

  Allocation _value;
  Allocator _owner;
};

inline std::tuple<vk::Buffer, vma::Allocation>
Allocator::create_buffer(const vk::BufferCreateInfo &buffer_create_info,
                         const Allocation_create_info &allocation_create_info,
                         Allocation_info *allocation_info) const {
  auto c_buffer_create_info = VkBufferCreateInfo{};
  std::memcpy(&c_buffer_create_info, &buffer_create_info,
              sizeof(VkBufferCreateInfo));
  auto c_buffer = VkBuffer{};
  auto c_allocation = VmaAllocation{};
  const auto result =
      vmaCreateBuffer(_value, &c_buffer_create_info, &allocation_create_info,
                      &c_buffer, &c_allocation, allocation_info);
  if (vk::Result{result} != vk::Result::eSuccess) {
    throw Buffer_creation_error{};
  }
  return std::tuple{vk::Buffer{c_buffer}, vma::Allocation{c_allocation}};
}

inline std::tuple<vk::UniqueBuffer, vma::Unique_allocation>
Allocator::create_buffer_unique(
    const vk::BufferCreateInfo &buffer_create_info,
    const Allocation_create_info &allocation_create_info,
    Allocation_info *allocation_info) const {
  auto allocator_info = VmaAllocatorInfo{};
  vmaGetAllocatorInfo(_value, &allocator_info);
  const auto [buffer, allocation] = create_buffer(
      buffer_create_info, allocation_create_info, allocation_info);
  return std::tuple{vk::UniqueBuffer{buffer, vk::Device{allocator_info.device}},
                    Unique_allocation{allocation, *this}};
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
