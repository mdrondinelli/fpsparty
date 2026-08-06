#ifndef FPSPARTY_MOTION_VECTOR_GLSL
#define FPSPARTY_MOTION_VECTOR_GLSL

vec3 motion_vector(vec4 current_clip, vec4 previous_clip) {
  const vec2 current_uv = current_clip.xy / current_clip.w * 0.5 + 0.5;
  const vec2 previous_uv = previous_clip.xy / previous_clip.w * 0.5 + 0.5;
  // previous_clip.w alone is already the linear view-space distance for
  // this reverse-Z infinite-far projection (the W-row is unaffected by
  // the reverse-Z trick) -- no need to divide through by it.
  const float previous_linear_depth = previous_clip.w;
  return vec3(previous_uv - current_uv, previous_linear_depth);
}

#endif
