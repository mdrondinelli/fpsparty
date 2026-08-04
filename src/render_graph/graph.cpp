#include "render_graph/graph.hpp"
#include "render_graph/resources.hpp"
#include <optional>
#include <variant>

namespace fpsparty::render_graph {

void Graph::add_pass(Node &node) { _nodes.push_back(&node); }

Symbolic_image Graph::allocate_image_symbol() noexcept {
  return {.id = _next_image_symbol++};
}

Symbolic_buffer Graph::allocate_buffer_symbol() noexcept {
  return {.id = _next_buffer_symbol++};
}

rc::Strong<graphics::Image> const &
Graph::resolve_image(Symbolic_image symbol) const {
  return _provided_images.at(symbol);
}

rc::Strong<graphics::Buffer> const &
Graph::resolve_buffer(Symbolic_buffer symbol) const {
  return _provided_buffers.at(symbol);
}

void Graph::execute(
  graphics::Work_recorder &recorder,
  std::initializer_list<std::pair<Symbolic_image, rc::Strong<graphics::Image>>>
    images,
  std::initializer_list<
    std::pair<Symbolic_buffer, rc::Strong<graphics::Buffer>>> buffers) {
  for (auto const &[symbol, image] : images) {
    _provided_images[symbol] = image;
  }
  for (auto const &[symbol, buffer] : buffers) {
    _provided_buffers[symbol] = buffer;
  }

  auto const find_last_write =
    [&](Builder::Entry const &entry) -> std::optional<Access> {
    if (auto const *image = std::get_if<Symbolic_image>(&entry.payload)) {
      auto const it = _last_image_write.find(resolve_image(*image));
      if (it != _last_image_write.end()) {
        return it->second;
      }
      return std::nullopt;
    }
    auto const &buffer = std::get<Symbolic_buffer>(entry.payload);
    auto const it = _last_buffer_write.find(resolve_buffer(buffer));
    if (it != _last_buffer_write.end()) {
      return it->second;
    }
    return std::nullopt;
  };

  auto const record_write = [&](Builder::Entry const &entry) {
    if (auto const *image = std::get_if<Symbolic_image>(&entry.payload)) {
      _last_image_write[resolve_image(*image)] = entry.access;
    } else {
      _last_buffer_write[resolve_buffer(std::get<Symbolic_buffer>(
        entry.payload))] = entry.access;
    }
  };

  for (auto *node : _nodes) {
    _builder._entries.clear();
    node->declare(_builder);

    auto src = Access{};
    auto dst = Access{};
    auto has_barrier = false;
    for (auto const &entry : _builder._entries) {
      if (entry.is_write) {
        continue;
      }
      if (auto const access = find_last_write(entry)) {
        src = src | *access;
        dst = dst | entry.access;
        has_barrier = true;
      }
    }
    if (has_barrier) {
      recorder.barrier(src, dst);
    }

    for (auto const &entry : _builder._entries) {
      if (entry.is_write) {
        record_write(entry);
      }
    }

    auto resources = Resources{_builder, *this};
    node->execute(recorder, resources);
  }
  _nodes.clear();
  _last_image_write.clear();
  _last_buffer_write.clear();
  _provided_images.clear();
  _provided_buffers.clear();
}

} // namespace fpsparty::render_graph
