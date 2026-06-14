#include "client/application.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "glfw.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "net/constants.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <tracy/Tracy.hpp>

using namespace fpsparty;

namespace {
constexpr auto server_ip = "127.0.0.1";

std::sig_atomic_t volatile signal_status{};

void handle_signal(int signal) { signal_status = signal; }
} // namespace

int main() {
  tracy::SetThreadName("Main Thread");
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  auto const enet_guard = enet::Initialization_guard{{}};
  auto const glfw_guard = glfw::Initialization_guard{{}};
  auto const vulkan_guard = graphics::Global_vulkan_state_guard{{}};
  auto application = client::Application{{
    .client_info =
      {
        .net_info = {},
        .max_buffered_ticks = 3,
        .tick_duration = constants::tick_duration,
      },
    .server_address =
      {
        .host = *enet::parse_ip(server_ip),
        .port = net::constants::port,
      },
    .window_info =
      {
        .width = 1024,
        .height = 768,
        .title = "FPS Party",
        .resizable = true,
        .client_api = glfw::Client_api::no_api,
      },
  }};
  std::cout << "Connecting to " << server_ip << " on port "
            << net::constants::port << ".\n";
  using Clock = std::chrono::high_resolution_clock;
  auto loop_duration = Clock::duration{};
  auto loop_time = Clock::now();
  while (!signal_status) {
    glfw::poll_events();
    auto const duration =
      std::chrono::duration_cast<std::chrono::duration<float>>(loop_duration)
        .count();
    if (duration > 0.0f && !application.update(duration)) {
      break;
    }
    FrameMark;
    auto const now = Clock::now();
    loop_duration = now - loop_time;
    loop_time = now;
  }
  graphics::Global_vulkan_state::get().device().waitIdle();
  std::cout << "Exiting.\n";
  return 0;
}
