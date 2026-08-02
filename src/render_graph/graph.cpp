#include "render_graph/graph.hpp"
#include "render_graph/resources.hpp"

namespace fpsparty::render_graph {

void Graph::add_pass(Node &node) { _nodes.push_back(&node); }

void Graph::execute(graphics::Work_recorder &recorder) {
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
      auto const it = _last_write.find(entry.descriptor->get_image());
      if (it == _last_write.end()) {
        continue;
      }
      src = src | it->second;
      dst = dst | entry.access;
      has_barrier = true;
    }
    if (has_barrier) {
      recorder.barrier(src, dst);
    }

    for (auto const &entry : _builder._entries) {
      if (entry.is_write) {
        _last_write[entry.descriptor->get_image()] = entry.access;
      }
    }

    auto resources = Resources{_builder};
    node->execute(recorder, resources);
  }
  _nodes.clear();
  _last_write.clear();
}

} // namespace fpsparty::render_graph
