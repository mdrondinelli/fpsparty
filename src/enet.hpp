#ifndef FPSPARTY_ENET_HPP
#define FPSPARTY_ENET_HPP

#include "flags.hpp"
#include <cstddef>
#include <cstdint>
#include <enet/enet.h>
#include <exception>
#include <optional>
#include <utility>

namespace fpsparty::enet {
class Initialization_guard {
public:
  struct Create_info {};

  class Construction_error : public std::exception {};

  constexpr Initialization_guard() noexcept = default;

  explicit Initialization_guard(Create_info) : _owning{true} {
    if (enet_initialize()) {
      throw Construction_error{};
    }
  }

  constexpr Initialization_guard(Initialization_guard &&other) noexcept
      : _owning{std::exchange(other._owning, false)} {}

  Initialization_guard &operator=(Initialization_guard &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Initialization_guard() {
    if (_owning) {
      enet_deinitialize();
    }
  }

private:
  constexpr void swap(Initialization_guard &other) noexcept {
    std::swap(_owning, other._owning);
  }

  bool _owning{};
};

class Peer {
  friend class Host;

public:
  constexpr operator ENetPeer *() const noexcept { return _value; }

  void disconnect(std::uint32_t data) const noexcept {
    enet_peer_disconnect(_value, data);
  }

  void disconnect_later(std::uint32_t data) const noexcept {
    enet_peer_disconnect_later(_value, data);
  }

  void disconnect_now(std::uint32_t data) const noexcept {
    enet_peer_disconnect_now(_value, data);
  }

  friend constexpr bool operator==(Peer lhs, Peer rhs) noexcept {
    return lhs._value == rhs._value;
  }

private:
  explicit constexpr Peer(ENetPeer *value) noexcept : _value{value} {}

  ENetPeer *_value;
};

enum class Packet_flag_bits {
  reliable = ENET_PACKET_FLAG_RELIABLE,
  unsequenced = ENET_PACKET_FLAG_UNSEQUENCED,
  no_allocate = ENET_PACKET_FLAG_NO_ALLOCATE,
  unreliable_fragment = ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT,
  sent = ENET_PACKET_FLAG_SENT,
};

using Packet_flags = Flags<Packet_flag_bits>;

constexpr Packet_flags operator|(Packet_flag_bits lhs,
                                 Packet_flag_bits rhs) noexcept {
  return Packet_flags{static_cast<Packet_flags::Value>(lhs) |
                      static_cast<Packet_flags::Value>(rhs)};
}

class Packet {
public:
  struct Create_info {
    const void *data;
    std::size_t data_length;
    Packet_flags flags{};
  };

  class Construction_error : std::exception {};

  constexpr Packet() noexcept = default;

  constexpr Packet(ENetPacket *value) noexcept : _value{value} {}

  constexpr operator ENetPacket *() const noexcept { return _value; }

private:
  ENetPacket *_value{};
};

inline Packet create_packet(const Packet::Create_info &create_info) {
  const auto value = enet_packet_create(
      create_info.data, create_info.data_length, create_info.flags);
  if (!value) {
    throw Packet::Construction_error{};
  }
  return value;
}

inline void destroy_packet(Packet packet) { enet_packet_destroy(packet); }

class Unique_packet {
  friend class Host;

public:
  constexpr Unique_packet() noexcept = default;

  constexpr explicit Unique_packet(Packet packet) : _value{packet} {}

  constexpr Unique_packet(Unique_packet &&other) noexcept
      : _value{std::exchange(other._value, nullptr)} {}

  Unique_packet &operator=(Unique_packet &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_packet() {
    if (_value) {
      destroy_packet(_value);
    }
  }

  constexpr operator bool() const noexcept { return _value; }

  constexpr Packet operator*() const noexcept { return _value; }

private:
  constexpr void swap(Unique_packet &other) noexcept {
    std::swap(_value, other._value);
  }

  Packet _value;
};

inline Unique_packet
create_packet_unique(const Packet::Create_info &create_info) {
  return Unique_packet{create_packet(create_info)};
}

using Address = ENetAddress;

inline std::optional<std::uint32_t> parse_ip(const char *ip) {
  auto address = Address{};
  if (enet_address_set_host_ip(&address, ip) == 0) {
    return address.host;
  } else {
    return std::nullopt;
  }
}

enum class Event_type {
  none = ENET_EVENT_TYPE_NONE,
  connect = ENET_EVENT_TYPE_CONNECT,
  disconnect = ENET_EVENT_TYPE_DISCONNECT,
  receive = ENET_EVENT_TYPE_RECEIVE,
};

struct Event {
  Event_type type;
  Peer peer;
  std::uint8_t channel_id;
  std::uint32_t data;
  Unique_packet packet;
};

class Host {
public:
  struct Create_info {
    std::optional<Address> address;
    std::size_t max_peer_count;
    std::size_t max_channel_count;
    std::uint32_t incoming_bandwidth;
    std::uint32_t outgoing_bandwidth;
  };

  class Construction_error : public std::exception {};

  class Out_of_peers_error : public std::exception {};

  class Service_error : public std::exception {};

  constexpr Host() noexcept = default;

  constexpr Host(ENetHost *value) noexcept : _value{value} {}

  constexpr operator ENetHost *() const noexcept { return _value; }

  Event service(std::uint32_t timeout) const {
    auto event = ENetEvent{};
    const auto service_result = enet_host_service(_value, &event, timeout);
    if (service_result < 0) {
      throw Service_error{};
    }
    return {
        .type = static_cast<Event_type>(event.type),
        .peer = Peer{event.peer},
        .channel_id = event.channelID,
        .data = event.data,
        .packet = Unique_packet{event.packet},
    };
  }

  Peer connect(const Address &address, std::size_t channel_count,
               std::uint32_t data) const {
    const auto peer = enet_host_connect(_value, &address, channel_count, data);
    if (!peer) {
      throw Out_of_peers_error{};
    }
    return Peer{peer};
  }

private:
  ENetHost *_value{};
};

inline Host create_host(const Host::Create_info &create_info) {
  const auto value = enet_host_create(
      create_info.address.has_value() ? &create_info.address.value() : nullptr,
      create_info.max_peer_count, create_info.max_channel_count,
      create_info.incoming_bandwidth, create_info.outgoing_bandwidth);
  if (!value) {
    throw Host::Construction_error{};
  }
  return value;
}

inline void destroy_host(Host host) { enet_host_destroy(host); }

class Unique_host {
public:
  constexpr Unique_host() noexcept = default;

  constexpr explicit Unique_host(Host value) noexcept : _value{value} {}

  constexpr Unique_host(Unique_host &&other) noexcept
      : _value{std::exchange(other._value, nullptr)} {}

  Unique_host &operator=(Unique_host &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_host() {
    if (_value) {
      enet_host_destroy(_value);
    }
  }

  constexpr Host operator*() const noexcept { return _value; }

  constexpr const Host *operator->() const noexcept { return &_value; }

private:
  constexpr void swap(Unique_host &other) noexcept {
    std::swap(_value, other._value);
  }

  Host _value{};
};

inline Unique_host create_host_unique(const Host::Create_info &create_info) {
  return Unique_host{create_host(create_info)};
}

struct Server_create_info {
  std::uint16_t port;
  std::size_t max_clients;
  std::size_t max_channels;
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
};

inline Host make_server_host(const Server_create_info &create_info) {
  return create_host({
      .address = Address{.host = ENET_HOST_ANY, .port = create_info.port},
      .max_peer_count = create_info.max_clients,
      .max_channel_count = create_info.max_channels,
      .incoming_bandwidth = create_info.incoming_bandwidth,
      .outgoing_bandwidth = create_info.outgoing_bandwidth,
  });
}

inline Unique_host
make_server_host_unique(const Server_create_info &create_info) {
  return Unique_host{make_server_host(create_info)};
}

struct Client_create_info {
  std::size_t max_channel_count;
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
};

inline Host make_client_host(const Client_create_info &create_info) {
  return create_host({
      .address = {},
      .max_peer_count = 1,
      .max_channel_count = create_info.max_channel_count,
      .incoming_bandwidth = create_info.incoming_bandwidth,
      .outgoing_bandwidth = create_info.outgoing_bandwidth,
  });
}

inline Unique_host
make_client_host_unique(const Client_create_info &create_info) {
  return Unique_host{make_client_host(create_info)};
}
} // namespace fpsparty::enet

#endif
