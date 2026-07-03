#include "descriptor_heap.hpp"

#include "global_vulkan_state.hpp"

#include <array>
#include <cassert>
#include <stdexcept>

namespace fpsparty::graphics::detail {

namespace {

auto constexpr combined_image_count = std::uint32_t{1024};
auto constexpr storage_image_count = std::uint32_t{1024};
auto constexpr combined_image_binding = std::uint32_t{0};
auto constexpr storage_image_binding = std::uint32_t{1};

vk::UniqueDescriptorSetLayout make_descriptor_set_layout() {
  auto const bindings = std::array{
    vk::DescriptorSetLayoutBinding{
      .binding = combined_image_binding,
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = combined_image_count,
      .stageFlags = vk::ShaderStageFlagBits::eAll,
    },
    vk::DescriptorSetLayoutBinding{
      .binding = storage_image_binding,
      .descriptorType = vk::DescriptorType::eStorageImage,
      .descriptorCount = storage_image_count,
      .stageFlags = vk::ShaderStageFlagBits::eAll,
    },
  };
  constexpr auto image_binding_flags =
    vk::DescriptorBindingFlagBits::ePartiallyBound |
    // vk::DescriptorBindingFlagBits::eUpdateAfterBind |
    vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;
  auto const binding_flags = std::array{
    vk::DescriptorBindingFlags{image_binding_flags},
    vk::DescriptorBindingFlags{image_binding_flags},
  };
  auto const binding_flags_info = vk::DescriptorSetLayoutBindingFlagsCreateInfo{
    .bindingCount = static_cast<std::uint32_t>(binding_flags.size()),
    .pBindingFlags = binding_flags.data(),
  };
  return Global_vulkan_state::get().device().createDescriptorSetLayoutUnique({
    .pNext = &binding_flags_info,
    // .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
    .bindingCount = static_cast<std::uint32_t>(bindings.size()),
    .pBindings = bindings.data(),
  });
}

vk::UniqueDescriptorPool make_descriptor_pool() {
  auto const pool_sizes = std::array{
    vk::DescriptorPoolSize{
      .type = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = combined_image_count,
    },
    vk::DescriptorPoolSize{
      .type = vk::DescriptorType::eStorageImage,
      .descriptorCount = storage_image_count,
    },
  };
  return Global_vulkan_state::get().device().createDescriptorPoolUnique({
    // .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
    .maxSets = 1,
    .poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  });
}

std::vector<vk::UniqueSampler> make_samplers() {
  auto const sampler_create_infos = std::array{
    vk::SamplerCreateInfo{},
    vk::SamplerCreateInfo{
      .addressModeU = vk::SamplerAddressMode::eClampToEdge,
      .addressModeV = vk::SamplerAddressMode::eClampToEdge,
      .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    },
    vk::SamplerCreateInfo{
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
    },
    vk::SamplerCreateInfo{
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .addressModeU = vk::SamplerAddressMode::eClampToEdge,
      .addressModeV = vk::SamplerAddressMode::eClampToEdge,
      .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    },
  };
  auto retval = std::vector<vk::UniqueSampler>{};
  retval.reserve(sampler_create_infos.size());
  for (auto const &sampler_create_info : sampler_create_infos) {
    retval.emplace_back(
      Global_vulkan_state::get().device().createSamplerUnique(
        sampler_create_info));
  }
  return retval;
}

void write_image(
  vk::DescriptorSet descriptor_set,
  std::uint32_t binding,
  std::uint32_t index,
  vk::DescriptorType descriptor_type,
  Image const &image,
  vk::Sampler sampler) {
  auto const image_info = vk::DescriptorImageInfo{
    .sampler = sampler,
    .imageView = get_image_vk_image_view(image),
    .imageLayout = vk::ImageLayout::eGeneral,
  };
  auto const write = vk::WriteDescriptorSet{
    .dstSet = descriptor_set,
    .dstBinding = binding,
    .dstArrayElement = index,
    .descriptorCount = 1,
    .descriptorType = descriptor_type,
    .pImageInfo = &image_info,
  };
  Global_vulkan_state::get().device().updateDescriptorSets({write}, {});
}

} // namespace

Descriptor_heap::Descriptor_heap(Descriptor_heap_create_info const &)
    : _vk_descriptor_set_layout{make_descriptor_set_layout()},
      _vk_samplers{make_samplers()},
      _vk_descriptor_pool{make_descriptor_pool()},
      _vk_descriptor_set{
        Global_vulkan_state::get().device().allocateDescriptorSets({
          .descriptorPool = *_vk_descriptor_pool,
          .descriptorSetCount = 1,
          .pSetLayouts = &*_vk_descriptor_set_layout,
        })[0]} {
  auto const fill_free_list =
    [](std::vector<std::uint32_t> &free_list, std::uint32_t count) {
      free_list.reserve(count);
      for (auto i = count; i != 0; --i) {
        free_list.push_back(i - 1);
      }
    };
  fill_free_list(_combined_image_free_list, combined_image_count);
  fill_free_list(_storage_image_free_list, storage_image_count);
}

u32 Descriptor_heap::alloc_sampled_image(Image const &image, Sampler sampler) {
  if (_combined_image_free_list.empty()) {
    throw std::runtime_error{"Descriptor heap is out of space"};
  }
  auto const handle = _combined_image_free_list.back();
  _combined_image_free_list.pop_back();
  write_image(
    _vk_descriptor_set,
    combined_image_binding,
    handle,
    vk::DescriptorType::eCombinedImageSampler,
    image,
    *_vk_samplers[static_cast<std::size_t>(sampler)]);
  return handle;
}

void Descriptor_heap::free_sampled_image(u32 handle) noexcept {
  auto const lock = std::scoped_lock{_mutex};
  _combined_image_free_list.push_back(handle);
}

u32 Descriptor_heap::alloc_storage_image(Image const &image) {
  if (_storage_image_free_list.empty()) {
    throw std::runtime_error{"Descriptor heap is out of space"};
  }
  auto const handle = _storage_image_free_list.back();
  _storage_image_free_list.pop_back();
  write_image(
    _vk_descriptor_set,
    storage_image_binding,
    handle,
    vk::DescriptorType::eStorageImage,
    image,
    {});
  return handle;
}

void Descriptor_heap::free_storage_image(u32 handle) noexcept {
  auto const lock = std::scoped_lock{_mutex};
  _storage_image_free_list.push_back(handle);
}

vk::DescriptorSetLayout get_descriptor_heap_vk_descriptor_set_layout(
  Descriptor_heap const &descriptor_heap) noexcept {
  return *descriptor_heap._vk_descriptor_set_layout;
}

vk::DescriptorSet get_descriptor_heap_vk_descriptor_set(
  Descriptor_heap const &descriptor_heap) noexcept {
  return descriptor_heap._vk_descriptor_set;
}

} // namespace fpsparty::graphics::detail
