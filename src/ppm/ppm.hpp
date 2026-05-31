#ifndef FPSPARTY_PPM_PPM_HPP
#define FPSPARTY_PPM_PPM_HPP

#include <cstddef>
#include <memory>
#include <span>

namespace fpsparty::ppm {
struct Ppm_image {
  std::unique_ptr<std::byte[]> data;
  int width;
  int height;
};

Ppm_image load_ppm(std::span<std::byte const> file);
} // namespace fpsparty::ppm

#endif
