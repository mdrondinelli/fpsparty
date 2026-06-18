#version 450

layout(push_constant) uniform Push_constants {
  layout(offset = 0) ivec2 resolution;
} push_constants;

const int crosshair_size = 32;
const int crosshair_thickness = 4;

ivec2 horizonal_quad_position(int idx) {
  const ivec2 min_position = push_constants.resolution / 2 - ivec2(crosshair_size / 2, crosshair_thickness / 2);
  const ivec2 max_position = min_position + ivec2(crosshair_size, crosshair_thickness);
  const ivec2 positions[4] = ivec2[](
    min_position,
    ivec2(max_position.x, min_position.y),
    ivec2(min_position.x, max_position.y),
    max_position
  );
  return positions[idx];
}

ivec2 vertical_quad_position(int idx) {
  const ivec2 min_position = push_constants.resolution / 2 - ivec2(crosshair_thickness / 2, crosshair_size / 2);
  const ivec2 max_position = min_position + ivec2(crosshair_thickness, crosshair_size);
  const ivec2 positions[4] = ivec2[](
    min_position,
    ivec2(max_position.x, min_position.y),
    ivec2(min_position.x, max_position.y),
    max_position
  );
  return positions[idx];
}

void main() {
  const ivec2 position = gl_VertexIndex < 4
    ? horizonal_quad_position(gl_VertexIndex)
    : vertical_quad_position(gl_VertexIndex - 4);
  const vec2 clip_position =
    vec2(position) / vec2(push_constants.resolution) * 2.0f -
    1.0f;
  gl_Position = vec4(clip_position, 0.0f, 1.0f);
}
