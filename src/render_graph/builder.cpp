#include "render_graph/builder.hpp"

namespace fpsparty::render_graph {

Resource_handle
Builder::read(rc::Strong<graphics::Descriptor> descriptor, Access access) {
  auto const index = static_cast<u32>(_entries.size());
  _entries.push_back({
    .descriptor = std::move(descriptor),
    .access = access,
    .is_write = false,
  });
  return {.index = index};
}

Resource_handle
Builder::write(rc::Strong<graphics::Descriptor> descriptor, Access access) {
  auto const index = static_cast<u32>(_entries.size());
  _entries.push_back({
    .descriptor = std::move(descriptor),
    .access = access,
    .is_write = true,
  });
  return {.index = index};
}

} // namespace fpsparty::render_graph
