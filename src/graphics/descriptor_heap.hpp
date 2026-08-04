#ifndef FPSPARTY_GRAPHICS_DESCRIPTOR_HEAP_HPP
#define FPSPARTY_GRAPHICS_DESCRIPTOR_HEAP_HPP

#include <mutex>
#include <memory>
#include <vector>

#include <int.hpp>
#include <rc.hpp>

#include "image.hpp"
#include "sampler.hpp"

namespace fpsparty::graphics::detail {

struct Descriptor_heap_create_info {};

class Descriptor_heap {
public:
  constexpr Descriptor_heap() noexcept = default;

  explicit Descriptor_heap(Descriptor_heap_create_info const &info);

  Descriptor_heap(Descriptor_heap const &other) = delete;

  Descriptor_heap &operator=(Descriptor_heap const &other) = delete;

  u32 alloc_sampled_image(Image const &image);

  void free_sampled_image(u32 handle) noexcept;

  u32 alloc_storage_image(Image const &image);

  void free_storage_image(u32 handle) noexcept;

private:
  friend vk::DescriptorSetLayout get_descriptor_heap_vk_descriptor_set_layout(
    Descriptor_heap const &descriptor_heap) noexcept;

  friend vk::DescriptorSet get_descriptor_heap_vk_descriptor_set(
    Descriptor_heap const &descriptor_heap) noexcept;

  vk::UniqueDescriptorSetLayout _vk_descriptor_set_layout{};
  std::vector<vk::UniqueSampler> _vk_samplers{};
  std::unique_ptr<Image> _null_image{};
  vk::UniqueDescriptorPool _vk_descriptor_pool{};
  vk::DescriptorSet _vk_descriptor_set{};
  std::vector<u32> _combined_image_free_list{};
  std::vector<u32> _storage_image_free_list{};
  std::mutex _mutex;
};

vk::DescriptorSetLayout get_descriptor_heap_vk_descriptor_set_layout(
  Descriptor_heap const &descriptor_heap) noexcept;

vk::DescriptorSet get_descriptor_heap_vk_descriptor_set(
  Descriptor_heap const &descriptor_heap) noexcept;

} // namespace fpsparty::graphics::detail

#endif
