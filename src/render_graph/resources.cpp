#include "render_graph/resources.hpp"
#include <variant>

namespace fpsparty::render_graph {

rc::Strong<graphics::Descriptor> const &
Resources::get_descriptor(Resource_handle handle) const {
  return std::get<rc::Strong<graphics::Descriptor>>(
    _builder->_entries.at(handle.index).payload);
}

rc::Strong<graphics::Image const> const &
Resources::get_image(Resource_handle handle) const {
  return std::get<rc::Strong<graphics::Image const>>(
    _builder->_entries.at(handle.index).payload);
}

rc::Strong<graphics::Buffer const> const &
Resources::get_buffer(Resource_handle handle) const {
  return std::get<rc::Strong<graphics::Buffer const>>(
    _builder->_entries.at(handle.index).payload);
}

} // namespace fpsparty::render_graph
