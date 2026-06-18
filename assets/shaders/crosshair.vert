#version 450

layout(push_constant) uniform Push_constants {
  layout(offset = 0) ivec2 resolution;
} push_constants;

void main() {
  const ivec2 min_position = (push_constants.resolution - ivec2(2)) / 2;
  const ivec2 max_position = min_position + ivec2(2);
  const ivec2 positions[4] = ivec2[](
    min_position,
    ivec2(max_position.x, min_position.y),
    ivec2(min_position.x, max_position.y),
    max_position
  );
  const vec2 clip_position =
    vec2(positions[gl_VertexIndex]) / vec2(push_constants.resolution) * 2.0f -
    1.0f;
  gl_Position = vec4(clip_position, 0.0f, 1.0f);
}
