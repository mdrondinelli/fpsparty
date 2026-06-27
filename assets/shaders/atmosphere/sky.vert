#version 450

layout(location = 0) out vec2 out_ndc;

void main() {
  const vec2 positions[3] = vec2[](
    vec2(-1.0f, -1.0f),
    vec2(3.0f, -1.0f),
    vec2(-1.0f, 3.0f)
  );
  out_ndc = positions[gl_VertexIndex];
  gl_Position = vec4(positions[gl_VertexIndex], 1.0f, 1.0f);
}
