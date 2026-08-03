#include "render_graph/builder.hpp"

namespace fpsparty::render_graph {

Resource_handle
Builder::read(rc::Strong<graphics::Descriptor> descriptor, Access access) {
  return add(std::move(descriptor), access, false);
}

Resource_handle
Builder::write(rc::Strong<graphics::Descriptor> descriptor, Access access) {
  return add(std::move(descriptor), access, true);
}

Resource_handle
Builder::read(rc::Strong<graphics::Image const> image, Access access) {
  return add(std::move(image), access, false);
}

Resource_handle
Builder::write(rc::Strong<graphics::Image const> image, Access access) {
  return add(std::move(image), access, true);
}

Resource_handle
Builder::read(rc::Strong<graphics::Buffer const> buffer, Access access) {
  return add(std::move(buffer), access, false);
}

Resource_handle
Builder::write(rc::Strong<graphics::Buffer const> buffer, Access access) {
  return add(std::move(buffer), access, true);
}

Resource_handle
Builder::add(Payload payload, Access access, bool is_write) {
  auto const index = static_cast<u32>(_entries.size());
  _entries.push_back({
    .payload = std::move(payload),
    .access = access,
    .is_write = is_write,
  });
  return {.index = index};
}

} // namespace fpsparty::render_graph
