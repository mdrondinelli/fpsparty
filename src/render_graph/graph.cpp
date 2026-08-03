#include "render_graph/graph.hpp"
#include "render_graph/resources.hpp"
#include <optional>
#include <variant>

namespace fpsparty::render_graph {

void Graph::add_pass(Node &node) { _nodes.push_back(&node); }

void Graph::execute(graphics::Work_recorder &recorder) {
  auto const find_last_write =
    [&](Builder::Entry const &entry) -> std::optional<Access> {
    if (auto const *descriptor =
          std::get_if<rc::Strong<graphics::Descriptor>>(&entry.payload)) {
      auto const it = _last_image_write.find((*descriptor)->get_image());
      if (it != _last_image_write.end()) {
        return it->second;
      }
      return std::nullopt;
    }
    if (auto const *image =
          std::get_if<rc::Strong<graphics::Image const>>(&entry.payload)) {
      auto const it = _last_image_write.find(*image);
      if (it != _last_image_write.end()) {
        return it->second;
      }
      return std::nullopt;
    }
    auto const &buffer =
      std::get<rc::Strong<graphics::Buffer const>>(entry.payload);
    auto const it = _last_buffer_write.find(buffer);
    if (it != _last_buffer_write.end()) {
      return it->second;
    }
    return std::nullopt;
  };

  auto const record_write = [&](Builder::Entry const &entry) {
    if (auto const *descriptor =
          std::get_if<rc::Strong<graphics::Descriptor>>(&entry.payload)) {
      _last_image_write[(*descriptor)->get_image()] = entry.access;
    } else if (
      auto const *image =
        std::get_if<rc::Strong<graphics::Image const>>(&entry.payload)) {
      _last_image_write[*image] = entry.access;
    } else {
      _last_buffer_write
        [std::get<rc::Strong<graphics::Buffer const>>(entry.payload)] =
        entry.access;
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

    auto resources = Resources{_builder};
    node->execute(recorder, resources);
  }
  _nodes.clear();
  _last_image_write.clear();
  _last_buffer_write.clear();
}

} // namespace fpsparty::render_graph
