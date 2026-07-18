#ifndef FPSPARTY_MOTION_VECTOR_GLSL
#define FPSPARTY_MOTION_VECTOR_GLSL

vec2 motion_vector(vec4 current_clip, vec4 previous_clip) {
  const vec2 current_uv = current_clip.xy / current_clip.w * 0.5 + 0.5;
  const vec2 previous_uv = previous_clip.xy / previous_clip.w * 0.5 + 0.5;
  return previous_uv - current_uv;
}

#endif
