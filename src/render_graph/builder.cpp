#include "render_graph/builder.hpp"

namespace fpsparty::render_graph {

Resource_handle Builder::read(Symbolic_image image, Access access) {
  return add(image, access, false);
}

Resource_handle Builder::write(Symbolic_image image, Access access) {
  return add(image, access, true);
}

Resource_handle Builder::read(Symbolic_buffer buffer, Access access) {
  return add(buffer, access, false);
}

Resource_handle Builder::write(Symbolic_buffer buffer, Access access) {
  return add(buffer, access, true);
}

Resource_handle
Builder::add(Payload payload, Access access, bool is_write) {
  auto const index = static_cast<u32>(_entries.size());
  _entries.push_back({
    .payload = payload,
    .access = access,
    .is_write = is_write,
  });
  return {.index = index};
}

} // namespace fpsparty::render_graph
