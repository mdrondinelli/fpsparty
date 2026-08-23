#!/usr/bin/env bash

set -euo pipefail

shader_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [[ -n "${GLSLC:-}" ]]; then
  glslc="$GLSLC"
elif [[ -n "${VULKAN_SDK:-}" && -x "$VULKAN_SDK/bin/glslc" ]]; then
  glslc="$VULKAN_SDK/bin/glslc"
else
  glslc="$(command -v glslc || true)"
fi

if [[ -z "$glslc" ]]; then
  echo "glslc not found; set GLSLC or VULKAN_SDK, or add it to PATH." >&2
  exit 1
fi

if [[ -n "${SPIRV_VAL:-}" ]]; then
  spirv_val="$SPIRV_VAL"
elif [[ -n "${VULKAN_SDK:-}" && -x "$VULKAN_SDK/bin/spirv-val" ]]; then
  spirv_val="$VULKAN_SDK/bin/spirv-val"
else
  spirv_val="$(command -v spirv-val || true)"
fi

shaders=(
  grid.vert
  grid.frag
  shader.vert
  shader.frag
  crosshair.vert
  crosshair.frag
  composite.vert
  composite.frag
  atmosphere/transmittance.comp
  atmosphere/sky_view.comp
  atmosphere/sky_irradiance.comp
  radiance.comp
  indirect_dispatch_args.comp
  distant_irradiance_light_pick.comp
  distant_irradiance_trace_sun.comp
  distant_irradiance_trace_sky.comp
  distant_irradiance_temporal.comp
  distant_irradiance_variance.comp
  distant_irradiance_spatial_filter.comp
  bin_rt_grid_entities.comp
)

for shader in "${shaders[@]}"; do
  source="$shader_dir/$shader"
  output="$source.spv"
  echo "Compiling $shader"
  "$glslc" \
    --target-env=vulkan1.4 \
    -I "$shader_dir" \
    "$source" \
    -o "$output"

  if [[ -n "$spirv_val" ]]; then
    "$spirv_val" --target-env vulkan1.4 --scalar-block-layout "$output"
  fi
done
