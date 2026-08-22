#include "render_graph/graph.hpp"
#include "render_graph/resources.hpp"
#include <algorithm>
#include <variant>

namespace fpsparty::render_graph {

Pass_handle Graph::add_pass(Node &node) {
  auto const handle = Pass_handle{.index = static_cast<u32>(_nodes.size())};
  _nodes.push_back(&node);
  return handle;
}

void Graph::set_disjoint(
  Pass_handle a, Pass_handle b, Symbolic_image resource) {
  _disjoint.push_back({
    .pass_a = a.index,
    .pass_b = b.index,
    .resource = resource,
  });
}

void Graph::set_disjoint(
  Pass_handle a, Pass_handle b, Symbolic_buffer resource) {
  _disjoint.push_back({
    .pass_a = a.index,
    .pass_b = b.index,
    .resource = resource,
  });
}

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

u32 Graph::resource_id(Resource_payload const &payload) {
  if (auto const *image = std::get_if<Symbolic_image>(&payload)) {
    auto const [it, inserted] =
      _image_ids.try_emplace(resolve_image(*image), _next_resource_id);
    if (inserted) {
      ++_next_resource_id;
    }
    return it->second;
  }
  auto const [it, inserted] = _buffer_ids.try_emplace(
    resolve_buffer(std::get<Symbolic_buffer>(payload)), _next_resource_id);
  if (inserted) {
    ++_next_resource_id;
  }
  return it->second;
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

  auto const node_count = _nodes.size();
  _builders.resize(node_count);
  for (auto i = std::size_t{}; i != node_count; ++i) {
    _builders[i]._entries.clear();
    _nodes[i]->declare(_builders[i]);
  }

  auto accesses = std::vector<std::vector<Scheduled_access>>(node_count);
  auto passes = std::vector<Scheduled_pass>{};
  passes.reserve(node_count);
  for (auto i = std::size_t{}; i != node_count; ++i) {
    accesses[i].reserve(_builders[i]._entries.size());
    for (auto const &entry : _builders[i]._entries) {
      accesses[i].push_back({
        .resource = resource_id(entry.payload),
        .access = entry.access,
        .is_write = entry.is_write,
      });
    }
    passes.push_back({.accesses = accesses[i]});
  }

  auto disjoint = std::vector<Disjoint_access>{};
  disjoint.reserve(_disjoint.size());
  for (auto const &declaration : _disjoint) {
    disjoint.push_back({
      .pass_a = declaration.pass_a,
      .pass_b = declaration.pass_b,
      .resource = resource_id(declaration.resource),
    });
  }

  auto const schedule = build_schedule(passes, disjoint);
  auto const layer_count =
    schedule.barriers.size() + (node_count != 0 ? 1 : 0);
  for (auto layer = std::size_t{}; layer != layer_count; ++layer) {
    if (layer != 0) {
      auto const &barrier = schedule.barriers[layer - 1];
      if (!barrier_record_is_empty(barrier)) {
        recorder.barrier(barrier.src, barrier.dst);
      }
    }
    for (auto i = std::size_t{}; i != node_count; ++i) {
      if (schedule.levels[i] != layer) {
        continue;
      }
      auto resources = Resources{_builders[i], *this};
      _nodes[i]->execute(recorder, resources);
    }
  }

  _nodes.clear();
  _disjoint.clear();
  _provided_images.clear();
  _provided_buffers.clear();
  _image_ids.clear();
  _buffer_ids.clear();
  _next_resource_id = 0;
}

} // namespace fpsparty::render_graph
