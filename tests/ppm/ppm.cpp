#include "ppm/ppm.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
std::vector<std::byte> bytes(std::string_view text) {
  auto result = std::vector<std::byte>{};
  result.reserve(text.size());
  for (auto const c : text) {
    result.push_back(static_cast<std::byte>(c));
  }
  return result;
}
} // namespace

TEST_CASE("P6 PPM loader returns dimensions and RGB raster bytes") {
  auto file = bytes("P6\n2 1\n255\n");
  file.push_back(static_cast<std::byte>(0x10));
  file.push_back(static_cast<std::byte>(0x20));
  file.push_back(static_cast<std::byte>(0x30));
  file.push_back(static_cast<std::byte>(0x40));
  file.push_back(static_cast<std::byte>(0x50));
  file.push_back(static_cast<std::byte>(0x60));

  auto image = fpsparty::ppm::load_ppm(file);

  CHECK(image.width == 2);
  CHECK(image.height == 1);
  auto const raster = std::span{image.data.get(), 6};
  CHECK(raster[0] == static_cast<std::byte>(0x10));
  CHECK(raster[1] == static_cast<std::byte>(0x20));
  CHECK(raster[2] == static_cast<std::byte>(0x30));
  CHECK(raster[3] == static_cast<std::byte>(0x40));
  CHECK(raster[4] == static_cast<std::byte>(0x50));
  CHECK(raster[5] == static_cast<std::byte>(0x60));
}

TEST_CASE("P6 PPM loader skips header comments") {
  auto file = bytes("P6\n# comment\n1 # width comment\n1\n255\nabc");

  auto image = fpsparty::ppm::load_ppm(file);

  CHECK(image.width == 1);
  CHECK(image.height == 1);
  CHECK(image.data[0] == static_cast<std::byte>('a'));
  CHECK(image.data[1] == static_cast<std::byte>('b'));
  CHECK(image.data[2] == static_cast<std::byte>('c'));
}

TEST_CASE("P6 PPM loader rejects unsupported files") {
  CHECK_THROWS_AS(fpsparty::ppm::load_ppm(bytes("P3\n1 1\n255\nabc")),
                  std::invalid_argument);
  CHECK_THROWS_AS(fpsparty::ppm::load_ppm(bytes("P6\n1 1\n256\nabcdef")),
                  std::invalid_argument);
  CHECK_THROWS_AS(fpsparty::ppm::load_ppm(bytes("P6\n1 1\n255\nab")),
                  std::invalid_argument);
  CHECK_THROWS_AS(fpsparty::ppm::load_ppm(bytes("P6\n0 1\n255\n")),
                  std::invalid_argument);
  CHECK_THROWS_AS(fpsparty::ppm::load_ppm(bytes("P6\n1 1\n127\nabc")),
                  std::invalid_argument);
}
