#ifndef FPSPARTY_GAME_ENTITY_WORLD_HPP
#define FPSPARTY_GAME_ENTITY_WORLD_HPP

#include "game/core/entity.hpp"
#include "rc.hpp"
#include "serial/reader.hpp"
#include "serial/writer.hpp"
#include <concepts>
#include <memory_resource>
#include <vector>

namespace fpsparty::game {
class Entity_world;

class Entity_dumper {
public:
  virtual ~Entity_dumper() = default;

  virtual Entity_type get_entity_type() const noexcept = 0;

  virtual void dump_entity(serial::Writer &writer,
                           const Entity &entity) const = 0;
};

class Entity_loader {
public:
  virtual ~Entity_loader() = default;

  virtual Entity_type get_entity_type() const noexcept = 0;

  virtual rc::Strong<Entity> create_entity(net::Entity_id entity_id) = 0;

  virtual void load_entity(serial::Reader &reader, Entity &entity,
                           const Entity_world &world) const = 0;
};

struct Entity_world_dump_info {
  serial::Writer *writer{};
  std::span<const Entity_dumper *const> dumpers{};
};

struct Entity_world_load_info {
  std::span<serial::Reader *const> readers{};
  std::span<Entity_loader *const> loaders{};
};

class Entity_world_load_error : public std::exception {};

class Entity_world {
public:
  void dump(const Entity_world_dump_info &info) const;

  void load(const Entity_world_load_info &info);

  void reset();

  bool add(const rc::Strong<Entity> &entity);

  bool remove(const rc::Strong<Entity> &entity) noexcept;

  template <std::derived_from<Entity> T>
  std::pmr::vector<rc::Strong<T>>
  get_entities_of_type(std::pmr::memory_resource *memory_resource =
                           std::pmr::get_default_resource()) const {
    auto retval = std::pmr::vector<rc::Strong<T>>{memory_resource};
    retval.reserve(count_entities_of_type<T>());
    for (const auto &entity : _entities) {
      if (auto typed_entity = entity.downcast<T>()) {
        retval.emplace_back(std::move(typed_entity));
      }
    }
    return retval;
  }

  template <std::derived_from<Entity> T>
  std::size_t count_entities_of_type() const noexcept {
    auto retval = std::size_t{};
    for (const auto &entity : _entities) {
      if (entity.downcast<T>()) {
        ++retval;
      }
    }
    return retval;
  }

  rc::Strong<Entity> get_entity_by_id(net::Entity_id id) const noexcept;

private:
  std::vector<rc::Strong<Entity>> _entities{};
};
} // namespace fpsparty::game

#endif
