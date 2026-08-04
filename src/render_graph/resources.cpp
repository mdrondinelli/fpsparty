#include "render_graph/resources.hpp"
#include "render_graph/graph.hpp"
#include <variant>

namespace fpsparty::render_graph {

rc::Strong<graphics::Descriptor> const &
Resources::get_descriptor(Resource_handle handle) const {
  return _graph->resolve_descriptor(std::get<Symbolic_descriptor>(
    _builder->_entries.at(handle.index).payload));
}

rc::Strong<graphics::Image> const &
Resources::get_image(Resource_handle handle) const {
  return _graph->resolve_image(
    std::get<Symbolic_image>(_builder->_entries.at(handle.index).payload));
}

rc::Strong<graphics::Buffer> const &
Resources::get_buffer(Resource_handle handle) const {
  return _graph->resolve_buffer(
    std::get<Symbolic_buffer>(_builder->_entries.at(handle.index).payload));
}

} // namespace fpsparty::render_graph
