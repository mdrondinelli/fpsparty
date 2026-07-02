#ifndef FPSPARTY_GRAPHICS_DESCRIPTOR_HEAP_HPP
#define FPSPARTY_GRAPHICS_DESCRIPTOR_HEAP_HPP

#include <cstdint>
#include <mutex>
#include <vector>

#include <rc.hpp>

#include "image.hpp"

namespace fpsparty::graphics::detail {

struct Descriptor_heap_create_info {
  std::uint32_t capacity{};
};

class Descriptor_heap {
public:
  constexpr Descriptor_heap() noexcept = default;

  explicit Descriptor_heap(Descriptor_heap_create_info const &info);

  Descriptor_heap(Descriptor_heap const &other) = delete;

  Descriptor_heap &operator=(Descriptor_heap const &other) = delete;

  std::uint32_t alloc();

  void free(std::uint32_t index) noexcept;

  void write_sampled_image(std::uint32_t index, Image const &image);

  void write_storage_image(std::uint32_t index, Image const &image);

private:
  friend vk::DescriptorSetLayout get_descriptor_heap_vk_descriptor_set_layout(
    Descriptor_heap const &descriptor_heap) noexcept;

  friend vk::DescriptorSet get_descriptor_heap_vk_descriptor_set(
    Descriptor_heap const &descriptor_heap) noexcept;

  vk::UniqueDescriptorSetLayout _vk_descriptor_set_layout{};
  vk::UniqueDescriptorPool _vk_descriptor_pool{};
  vk::DescriptorSet _vk_descriptor_set{};
  std::vector<vk::UniqueSampler> _vk_samplers{};
  std::vector<std::uint32_t> _free_list{};
  std::mutex _mutex;
};

vk::DescriptorSetLayout get_descriptor_heap_vk_descriptor_set_layout(
  Descriptor_heap const &descriptor_heap) noexcept;

vk::DescriptorSet get_descriptor_heap_vk_descriptor_set(
  Descriptor_heap const &descriptor_heap) noexcept;

} // namespace fpsparty::graphics::detail

#endif
