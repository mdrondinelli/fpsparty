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
  auto temp_entities = std::vector<rc::Strong<Entity>>{};
  temp_entities.reserve(_entities.capacity());
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
          const auto new_entity = (*loader_it)->create_entity(*entity_id);
          _entities.emplace_back(new_entity);
          return new_entity;
        }
      }();
      (*loader_it)->load_entity(*reader, *entity, *this);
      temp_entities.emplace_back(std::move(entity));
    }
  }
  std::swap(_entities, temp_entities);
  for (const auto &entity : temp_entities) {
    if (!get_entity_by_id(entity->get_entity_id())) {
      detail::on_remove_entity(*entity);
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

bool Entity_world::add(const rc::Strong<Entity> &entity) {
  const auto it = std::ranges::find(_entities, entity);
  if (it == _entities.end()) {
    _entities.emplace_back(entity);
    return true;
  } else {
    return false;
  }
}

bool Entity_world::remove(const rc::Strong<Entity> &entity) noexcept {
  const auto it = std::ranges::find(_entities, entity);
  if (it != _entities.end()) {
    detail::on_remove_entity(**it);
    algorithms::unordered_erase_at(_entities, it);
    return true;
  } else {
    return false;
  }
}

rc::Strong<Entity>
Entity_world::get_entity_by_id(net::Entity_id id) const noexcept {
  const auto it =
      std::ranges::find_if(_entities, [&](const rc::Strong<Entity> &entity) {
        return entity->get_entity_id() == id;
      });
  if (it != _entities.end()) {
    return *it;
  } else {
    return nullptr;
  }
}
} // namespace fpsparty::game
