#include <catch2/catch_test_macros.hpp>

#include <client/block_model_registry.hpp>

namespace fpsparty::client {

TEST_CASE("Block model registry grows and still terminates missing lookups") {
  auto registry = Block_model_registry{};
  for (auto data = 0; data != 9; ++data) {
    registry.add(game::Block::placeholder, data, Block_model{});
  }

  for (auto data = 0; data != 9; ++data) {
    CHECK(registry.get(game::Block::placeholder, data) != nullptr);
  }
  CHECK(registry.get(game::Block::air, 0) == nullptr);
}

} // namespace fpsparty::client
