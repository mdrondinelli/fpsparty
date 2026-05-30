#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 out_color;

struct Vertex {
  float position[3];
  float color[3];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
readonly buffer Vertex_buffer {
  Vertex vertices[];
};

layout(push_constant) uniform Push_constants {
  layout(offset = 0) mat4 model_view_projection_matrix;
  layout(offset = 64) Vertex_buffer vertex_buffer;
} push_constants;

void main() {
  Vertex vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
  gl_Position =
    push_constants.model_view_projection_matrix *
    vec4(vertex.position[0], vertex.position[1], vertex.position[2], 1.0);
  out_color = vec3(vertex.color[0], vertex.color[1], vertex.color[2]);
}
