#ifndef FPSPARTY_GAME_ENTITY_HPP
#define FPSPARTY_GAME_ENTITY_HPP

#include "game/core/entity_type.hpp"
#include "net/core/entity_id.hpp"
#include <concepts>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <vector>

namespace fpsparty::game {
class Entity;
template <typename T> class Entity_owner;
template <typename T> class Entity_factory;

namespace detail {
bool get_entity_remove_flag(const Entity &entity) noexcept;

void set_entity_remove_flag(Entity &entity, bool value) noexcept;

void on_remove_entity(Entity &entity) noexcept;

template <typename T> Entity_owner<T> acquire_entity(T *entity) noexcept {
  return Entity_owner<T>{entity};
}

template <typename T> T *release_entity(Entity_owner<T> &owner) noexcept {
  const auto entity = owner.get();
  owner._entity = nullptr;
  return entity;
}
} // namespace detail

class Entity_remove_listener {
public:
  virtual ~Entity_remove_listener() = default;

  virtual void on_remove_entity() = 0;
};

class Entity {
public:
  explicit Entity(Entity_type type, net::Entity_id id) noexcept;

  Entity(const Entity &other) = delete;

  Entity &operator=(const Entity &other) = delete;

  virtual ~Entity() = default;

  Entity_type get_entity_type() const noexcept;

  net::Entity_id get_entity_id() const noexcept;

  bool add_remove_listener(Entity_remove_listener *listener);

  bool remove_remove_listener(Entity_remove_listener *listener) noexcept;

protected:
  virtual void on_remove() = 0;

private:
  template <typename T> friend class Entity_owner;

  template <typename T> friend class Entity_factory;

  friend bool detail::get_entity_remove_flag(const Entity &entity) noexcept;

  friend void
  detail::set_entity_remove_flag(Entity &entity, bool value) noexcept;

  friend void detail::on_remove_entity(Entity &entity) noexcept;

  std::pmr::memory_resource *_memory_resource{};
  Entity_type _entity_type{};
  net::Entity_id _entity_id{};
  bool _remove_flag{};
  std::vector<Entity_remove_listener *> _removal_listeners{};
};

template <typename T> class Entity_owner {
public:
  constexpr Entity_owner() noexcept = default;

  constexpr Entity_owner(std::nullptr_t) noexcept {}

  constexpr Entity_owner(Entity_owner &&other) noexcept
      : _entity{std::exchange(other._entity, nullptr)} {}

  template <typename U>
    requires std::derived_from<U, T>
  constexpr Entity_owner(Entity_owner<U> &&other) noexcept
      : _entity{detail::release_entity(other)} {}

  constexpr Entity_owner &operator=(Entity_owner &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Entity_owner() {
    if (_entity) {
      auto allocator =
        std::pmr::polymorphic_allocator{_entity->_memory_resource};
      allocator.delete_object(_entity);
    }
  }

  constexpr operator bool() const noexcept { return _entity != nullptr; }

  constexpr T &operator*() const noexcept { return *_entity; }

  constexpr T *operator->() const noexcept { return _entity; }

  T *get() const noexcept { return _entity; }

  template <std::derived_from<T> U> Entity_owner<U> static_downcast() noexcept {
    return detail::acquire_entity(
      static_cast<U *>(detail::release_entity(*this)));
  }

  template <std::derived_from<T> U>
  Entity_owner<U> dynamic_downcast() noexcept {
    if (const auto result = dynamic_cast<U *>(_entity)) {
      detail::release_entity(*this);
      return detail::acquire_entity(result);
    } else {
      return nullptr;
    }
  }

private:
  friend Entity_owner<T> constexpr detail::acquire_entity(T *entity) noexcept;

  friend constexpr T *detail::release_entity(Entity_owner<T> &entity) noexcept;

  constexpr explicit Entity_owner(T *entity) noexcept : _entity{entity} {}

  constexpr void swap(Entity_owner &other) noexcept {
    std::swap(_entity, other._entity);
  }

  T *_entity{};
};

template <typename T> class Entity_factory {
public:
  Entity_factory() noexcept
      : Entity_factory{std::pmr::get_default_resource()} {}

  explicit Entity_factory(
    std::pmr::memory_resource *upstream_memory_resource) noexcept
      : _memory_resource{std::make_unique<std::pmr::synchronized_pool_resource>(
          std::pmr::pool_options{
            .largest_required_pool_block = sizeof(T),
          },
          upstream_memory_resource)} {}

  template <typename... Args> Entity_owner<T> create(Args &&...args) {
    auto allocator = std::pmr::polymorphic_allocator{_memory_resource.get()};
    const auto entity = allocator.new_object<T>(std::forward<Args>(args)...);
    entity->_memory_resource = _memory_resource.get();
    return detail::acquire_entity(entity);
  }

private:
  std::unique_ptr<std::pmr::synchronized_pool_resource> _memory_resource;
};

} // namespace fpsparty::game

#endif
