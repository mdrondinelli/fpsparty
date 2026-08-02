#include "render_graph/resources.hpp"

namespace fpsparty::render_graph {

rc::Strong<graphics::Descriptor> const &
Resources::get(Resource_handle handle) const {
  return _builder->_entries.at(handle.index).descriptor;
}

} // namespace fpsparty::render_graph
