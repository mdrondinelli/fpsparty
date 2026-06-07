#ifndef FPSPARTY_CLIENT_APPLICATION_HPP
#define FPSPARTY_CLIENT_APPLICATION_HPP

#include "client/client.hpp"
#include "enet.hpp"
#include "glfw.hpp"
#include <memory>

namespace fpsparty::client {
struct Application_create_info {
  Client_create_info client_info;
  enet::Address server_address;
  glfw::Window::Create_info window_info;
};

class Application {
public:
  explicit Application(Application_create_info const &create_info);

  ~Application();

  Application(Application const &other) = delete;

  Application &operator=(Application const &other) = delete;

  bool update(float duration);

private:
  class Impl;

  std::unique_ptr<Impl> _impl;
};
} // namespace fpsparty::client

#endif
