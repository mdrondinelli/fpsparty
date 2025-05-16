#include "constants.hpp"
#include "enet.hpp"
#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace fpsparty;

namespace {
volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto enet_guard = enet::Initialization_guard{{}};
  const auto server = enet::make_server_host({
      .port = constants::port,
      .max_clients = constants::max_clients,
      .max_channels = 1,
  });
  std::cout << "Server running on port " << constants::port << ".\n";
  auto peers = std::vector<enet::Peer>{};
  peers.reserve(constants::max_clients);
  while (!signal_status) {
    const auto event = server.service(0);
    switch (event.type) {
    case enet::Event_type::none:
      break;
    case enet::Event_type::connect:
      std::cout << "Got connect event.\n";
      peers.emplace_back(event.peer);
      break;
    case enet::Event_type::disconnect:
      std::cout << "Got disconnect event.\n";
      {
        const auto it = std::ranges::find(peers, event.peer);
        assert(it != peers.end());
        peers.erase(it);
      }
      break;
    case enet::Event_type::receive:
      std::cout << "Got receive event.\n";
      break;
    }
  }
  if (signal_status == SIGINT) {
    if (!peers.empty()) {
      std::cout << "Disconnecting " << peers.size() << " clients.\n";
      for (const auto peer : peers) {
        peer.disconnect(0);
      }
      do {
        const auto event = server.service(1000);
        if (event.type == enet::Event_type::disconnect) {
          std::cout << "Got disconnect event.\n";
          const auto it = std::ranges::find(peers, event.peer);
          assert(it != peers.end());
          peers.erase(it);
        }
      } while (!peers.empty());
    }
  }
  std::cout << "Exiting.\n";
  return 0;
}
