#include "descriptor_heap.hpp"

#include "global_vulkan_state.hpp"
#include "image_format.hpp"

#include <array>
#include <cassert>
#include <stdexcept>

namespace fpsparty::graphics::detail {

namespace {

constexpr auto combined_image_binding = std::uint32_t{0};
constexpr auto rgba8_storage_image_binding = std::uint32_t{1};
constexpr auto rgba16f_storage_image_binding = std::uint32_t{2};
constexpr auto rgba32f_storage_image_binding = std::uint32_t{3};
constexpr auto sampler_count = std::size_t{4};

std::uint32_t get_storage_image_binding(Image_format format) {
  switch (format) {
  case Image_format::r16g16b16a16_sfloat:
    return rgba16f_storage_image_binding;
  case Image_format::r32g32b32a32_sfloat:
    return rgba32f_storage_image_binding;
  default:
    throw std::runtime_error{"Unsupported storage image descriptor format."};
  }
}

vk::UniqueDescriptorSetLayout
make_descriptor_set_layout(std::uint32_t capacity) {
  auto const bindings = std::array{
    vk::DescriptorSetLayoutBinding{
      .binding = combined_image_binding,
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = capacity,
      .stageFlags = vk::ShaderStageFlagBits::eAll,
    },
    vk::DescriptorSetLayoutBinding{
      .binding = rgba8_storage_image_binding,
      .descriptorType = vk::DescriptorType::eStorageImage,
      .descriptorCount = capacity,
      .stageFlags = vk::ShaderStageFlagBits::eAll,
    },
    vk::DescriptorSetLayoutBinding{
      .binding = rgba16f_storage_image_binding,
      .descriptorType = vk::DescriptorType::eStorageImage,
      .descriptorCount = capacity,
      .stageFlags = vk::ShaderStageFlagBits::eAll,
    },
    vk::DescriptorSetLayoutBinding{
      .binding = rgba32f_storage_image_binding,
      .descriptorType = vk::DescriptorType::eStorageImage,
      .descriptorCount = capacity,
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

vk::UniqueDescriptorPool make_descriptor_pool(std::uint32_t capacity) {
  auto const pool_sizes = std::array{
    vk::DescriptorPoolSize{
      .type = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = capacity,
    },
    vk::DescriptorPoolSize{
      .type = vk::DescriptorType::eStorageImage,
      .descriptorCount = capacity * 3,
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

Descriptor_heap::Descriptor_heap(Descriptor_heap_create_info const &info)
    : _vk_descriptor_set_layout{make_descriptor_set_layout(info.capacity)},
      _vk_descriptor_pool{make_descriptor_pool(info.capacity)},
      _vk_descriptor_set{
        Global_vulkan_state::get().device().allocateDescriptorSets({
          .descriptorPool = *_vk_descriptor_pool,
          .descriptorSetCount = 1,
          .pSetLayouts = &*_vk_descriptor_set_layout,
        })[0]},
      _vk_samplers{make_samplers()} {
  _free_list.reserve(info.capacity);
  for (auto i = info.capacity; i != 0; --i) {
    _free_list.push_back(i - 1);
  }
}

std::uint32_t Descriptor_heap::alloc() {
  auto const lock = std::scoped_lock{_mutex};
  if (_free_list.empty()) {
    throw std::runtime_error{"Descriptor heap is out of space"};
  }
  auto const retval = _free_list.back();
  _free_list.pop_back();
  return retval;
}

void Descriptor_heap::free(std::uint32_t index) noexcept {
  auto const lock = std::scoped_lock{_mutex};
  _free_list.push_back(index);
}

void Descriptor_heap::write_sampled_image(
  std::uint32_t index, Image const &image, Sampler sampler) {
  auto const sampler_index = static_cast<std::size_t>(sampler);
  assert(sampler_index < sampler_count);
  write_image(
    _vk_descriptor_set,
    combined_image_binding,
    index,
    vk::DescriptorType::eCombinedImageSampler,
    image,
    *_vk_samplers[sampler_index]);
}

void Descriptor_heap::write_storage_image(
  std::uint32_t index, Image const &image) {
  write_image(
    _vk_descriptor_set,
    get_storage_image_binding(image.get_format()),
    index,
    vk::DescriptorType::eStorageImage,
    image,
    vk::Sampler{});
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
