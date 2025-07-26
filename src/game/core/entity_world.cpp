#include "entity_world.hpp"
#include "algorithms/unordered_erase.hpp"
#include "game/core/entity.hpp"
#include "serial/serialize.hpp"
#include <Eigen/Dense>
#include <algorithm>
#include <iostream>

namespace fpsparty::game {
void Entity_world::dump(const Entity_world_dump_info &info) const {
  using serial::serialize;
  auto dumpable_entity_count = std::size_t{};
  for (const auto &entity : _entities) {
    const auto entity_type = entity->get_entity_type();
    const auto dumper_it =
        std::ranges::find_if(info.dumpers, [&](const Entity_dumper *dumper) {
          return dumper->get_entity_type() == entity_type;
        });
    if (dumper_it != info.dumpers.end()) {
      ++dumpable_entity_count;
    }
  }
  serialize<std::uint32_t>(*info.writer, dumpable_entity_count);
  for (const auto &entity : _entities) {
    const auto entity_type = entity->get_entity_type();
    const auto dumper_it =
        std::ranges::find_if(info.dumpers, [&](const Entity_dumper *dumper) {
          return dumper->get_entity_type() == entity_type;
        });
    if (dumper_it != info.dumpers.end()) {
      serialize<Entity_type>(*info.writer, entity_type);
      serialize<net::Entity_id>(*info.writer, entity->get_entity_id());
      (*dumper_it)->dump_entity(*info.writer, *entity);
    }
  }
}

void Entity_world::load(const Entity_world_load_info &info) {
  using serial::deserialize;
  for (const auto &entity : _entities) {
    detail::set_entity_remove_flag(*entity, true);
  }
  for (const auto &reader : info.readers) {
    const auto entity_count = deserialize<std::uint32_t>(*reader);
    if (!entity_count) {
      std::cerr << "Failed to deserialize entity count.\n";
      throw Entity_world_load_error{};
    }
    for (auto i = std::uint32_t{}; i != *entity_count; ++i) {
      const auto entity_type = deserialize<Entity_type>(*reader);
      if (!entity_type) {
        std::cerr << "Failed to deserialize entity type.\n";
        throw Entity_world_load_error{};
      }
      const auto loader_it =
          std::ranges::find_if(info.loaders, [&](const Entity_loader *loader) {
            return loader->get_entity_type() == *entity_type;
          });
      if (loader_it == info.loaders.end()) {
        throw Entity_world_load_error{};
      }
      const auto entity_id = deserialize<net::Entity_id>(*reader);
      if (!entity_id) {
        std::cerr << "Failed to deserialize entity id.\n";
        throw Entity_world_load_error{};
      }
      auto entity = [&]() {
        const auto existing_entity = get_entity_by_id(*entity_id);
        if (existing_entity) {
          return existing_entity;
        } else {
          auto new_entity = (*loader_it)->create_entity(*entity_id);
          const auto new_entity_raw = new_entity.get();
          _entities.emplace_back(std::move(new_entity));
          return new_entity_raw;
        }
      }();
      detail::set_entity_remove_flag(*entity, false);
      (*loader_it)->load_entity(*reader, *entity, *this);
    }
  }
  for (auto it = _entities.begin(); it != _entities.end();) {
    if (detail::get_entity_remove_flag(**it)) {
      detail::on_remove_entity(**it);
      *it = std::move(_entities.back());
      _entities.pop_back();
    } else {
      ++it;
    }
  }
}

void Entity_world::reset() {
  while (!_entities.empty()) {
    const auto entity = std::move(_entities.back());
    detail::on_remove_entity(*entity);
    _entities.pop_back();
  }
}

void Entity_world::add(Entity_owner<Entity> entity) {
  _entities.emplace_back(std::move(entity));
}

Entity_owner<Entity> Entity_world::remove(const Entity *entity) noexcept {
  const auto it = std::ranges::find_if(
      _entities, [&](const Entity_owner<Entity> &owned_entity) {
        return owned_entity.get() == entity;
      });
  if (it != _entities.end()) {
    auto entity = std::move(*it);
    detail::on_remove_entity(*entity);
    algorithms::unordered_erase_at(_entities, it);
    return entity;
  } else {
    return nullptr;
  }
}

Entity *Entity_world::get_entity_by_id(net::Entity_id id) const noexcept {
  const auto it =
      std::ranges::find_if(_entities, [&](const Entity_owner<Entity> &entity) {
        return entity->get_entity_id() == id;
      });
  if (it != _entities.end()) {
    return it->get();
  } else {
    return nullptr;
  }
}
} // namespace fpsparty::game
