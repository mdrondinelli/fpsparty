#include "client/passes/rt_entity_binning_pass.hpp"
#include "client/rt_entity.hpp"
#include "client/rt_math.hpp"
#include "math/mat.hpp"
#include "math/transforms.hpp"
#include "render_graph/access.hpp"
#include <cstring>
#include <utility>
#include <vector>

namespace fpsparty::client::passes {

Rt_entity_binning_pass::Rt_entity_binning_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline)
    : _pipeline{std::move(pipeline)} {}

bool Rt_entity_binning_pass::update(Rt_entity_binning_pass_inputs inputs) {
  _inputs = std::move(inputs);
  auto const &session = _inputs.client->get_session();
  return session && _inputs.grid_mesh && _inputs.grid_mesh->is_uploaded();
}

void Rt_entity_binning_pass::declare(render_graph::Builder &builder) {
  _binning_buffer_handle = builder.write(
    _inputs.binning_buffer, render_graph::access::compute_storage_write);
}

void Rt_entity_binning_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const &binning_buffer = resources.get_buffer(_binning_buffer_handle);
  auto const &session = _inputs.client->get_session();
  auto const &boxes = session->get_scene().get_current_frame().boxes;
  auto entities = std::vector<Rt_entity>{};
  entities.reserve(boxes.size());
  for (auto const &box : boxes) {
    auto rotation = math::mat4::Identity().eval();
    rotation.block<3, 3>(0, 0) = box.orientation.toRotationMatrix();
    auto const model =
      (math::translation_matrix(box.position) * rotation).eval();
    auto const inverse_model = model.inverse().eval();
    auto const half_extents = box.half_extents.cwiseAbs().eval();
    entities.push_back({
      .model = make_rt_matrix_rows(model),
      .inverse_model = make_rt_matrix_rows(inverse_model),
      .half_extents =
        math::vec4{half_extents.x(), half_extents.y(), half_extents.z(), 0.0f},
      .albedo = math::vec4{0.3f, 0.3f, 0.3f, 0.0f},
    });
  }
  auto const entity_memory = _inputs.entity_buffer->map();
  auto const entity_count = static_cast<std::uint32_t>(entities.size());
  std::memcpy(entity_memory.get().data(), &entity_count, sizeof(entity_count));
  if (!entities.empty()) {
    std::memcpy(
      entity_memory.get().data() + 16,
      entities.data(),
      entities.size() * sizeof(Rt_entity));
  }
  auto const layout =
    make_rt_entity_binning_buffer_layout(_inputs.grid_mesh->get_rt_chunk_count());
  auto const entity_grid_memory = binning_buffer->map();
  std::memset(
    entity_grid_memory.get().data() + layout.nodes_offset,
    0,
    sizeof(std::uint32_t));

  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, _inputs.grid_mesh->get_rt_block_grid_buffer());
  recorder.push_buffer_reference(8, _inputs.entity_buffer);
  recorder.push_buffer_reference(16, binning_buffer, layout.grid_offset);
  recorder.push_buffer_reference(24, binning_buffer, layout.nodes_offset);
  recorder.dispatch(_inputs.grid_mesh->get_rt_chunk_count(), 1, 1);
}

} // namespace fpsparty::client::passes
