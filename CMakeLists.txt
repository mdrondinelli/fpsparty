cmake_minimum_required(VERSION 3.22.0)
project(fpsparty VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The non-MSVC build passes -stdlib=libc++ and links libc++abi
# unconditionally, so it needs Clang. Fail with one sentence rather than a
# wall of unrelated errors.
if (NOT MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR
    "fpsparty requires Clang (builds against libc++), got "
    "${CMAKE_CXX_COMPILER_ID}. Configure with a preset, or pass "
    "-DCMAKE_CXX_COMPILER=clang++-21 -DCMAKE_C_COMPILER=clang-21.")
endif()

# Four configurations, none of which map onto CMake's stock set.
# Development is the daily driver -- optimized enough to stay interactive,
# but with assertions and Vulkan validation live, so NDEBUG must stay off
# and RelWithDebInfo will not do. Profiling matches Release codegen
# exactly so a capture reflects what ships, plus symbols and Tracy.
set(fpsparty_build_types Debug Development Profiling Release)
get_property(fpsparty_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (fpsparty_multi_config)
  set(CMAKE_CONFIGURATION_TYPES "${fpsparty_build_types}" CACHE STRING "" FORCE)
elseif (NOT CMAKE_BUILD_TYPE)
  # Configuring without a preset lands on Development rather than on
  # whatever an empty CMAKE_BUILD_TYPE happens to mean.
  set(CMAKE_BUILD_TYPE Development CACHE STRING "" FORCE)
  message(STATUS "No build type set; defaulting to Development.")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${fpsparty_build_types})

if (MSVC)
  set(fpsparty_flags_debug "/Od /Zi")
  set(fpsparty_flags_development "/O2 /Zi")
  set(fpsparty_flags_profiling "/O2 /Zi /DNDEBUG")
  set(fpsparty_flags_release "/O2 /DNDEBUG")
else()
  set(fpsparty_flags_debug "-O0 -g")
  set(fpsparty_flags_development "-O2 -g")
  set(fpsparty_flags_profiling "-O3 -g -DNDEBUG")
  set(fpsparty_flags_release "-O3 -DNDEBUG")
endif()
foreach(lang C CXX)
  set(CMAKE_${lang}_FLAGS_DEBUG
    "${fpsparty_flags_debug}" CACHE STRING "" FORCE)
  set(CMAKE_${lang}_FLAGS_DEVELOPMENT
    "${fpsparty_flags_development}" CACHE STRING "" FORCE)
  set(CMAKE_${lang}_FLAGS_PROFILING
    "${fpsparty_flags_profiling}" CACHE STRING "" FORCE)
  set(CMAKE_${lang}_FLAGS_RELEASE
    "${fpsparty_flags_release}" CACHE STRING "" FORCE)
endforeach()
# The custom configurations need linker flag variables to exist, or link
# rules come out empty-handed for them.
foreach(kind EXE SHARED MODULE STATIC)
  set(CMAKE_${kind}_LINKER_FLAGS_DEVELOPMENT "" CACHE STRING "" FORCE)
  set(CMAKE_${kind}_LINKER_FLAGS_PROFILING "" CACHE STRING "" FORCE)
endforeach()

# Vulkan validation layers plus a debug messenger. Costs real frame time,
# so profiling builds leave it off.
option(FPSPARTY_VULKAN_VALIDATION "Enable Vulkan validation layers" OFF)
# Object names and command labels via VK_EXT_debug_utils. Wanted in
# profiling builds too -- captures are where the names pay off -- so it is
# gated separately from validation.
option(FPSPARTY_VULKAN_DEBUG_NAMES "Enable Vulkan object names and labels" OFF)
option(FPSPARTY_TRACY "Enable Tracy profiler instrumentation" OFF)

if (FPSPARTY_TRACY)
  add_compile_definitions(TRACY_ENABLE)
endif()
if (FPSPARTY_VULKAN_VALIDATION)
  add_compile_definitions(FPSPARTY_VULKAN_VALIDATION)
endif()
if (FPSPARTY_VULKAN_DEBUG_NAMES)
  add_compile_definitions(FPSPARTY_VULKAN_DEBUG_NAMES)
endif()

# dependencies
add_subdirectory(extern/glfw-3.4)
add_subdirectory(extern/enet)
add_subdirectory(extern/volk-1.4.350)
add_subdirectory(extern/VulkanMemoryAllocator-3.3.0)
add_subdirectory(extern/magic_enum-0.9.7)
add_subdirectory(extern/Catch2-3.13.0)
enable_testing()
add_library(
  tracy
  extern/tracy-0.13.1/public/TracyClient.cpp
)
add_library(
  spirv-reflect
  extern/SPIRV-Reflect/spirv_reflect.cpp
)
target_include_directories(
  spirv-reflect
  SYSTEM
  PUBLIC
  ${CMAKE_SOURCE_DIR}/extern/SPIRV-Reflect
)

# libnet
add_library(
  net
  src/net/client.cpp
  src/net/server.cpp
)
target_include_directories(
  net
  SYSTEM
  PUBLIC
  ${CMAKE_SOURCE_DIR}/extern/eigen-3.4.0
  ${CMAKE_SOURCE_DIR}/extern/magic_enum-0.9.7/include
  ${CMAKE_SOURCE_DIR}/extern/enet/include
)
target_include_directories(
  net
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(net PUBLIC enet)

# libgame
add_library(
  game
  src/game/block_bounds_registry.cpp
  src/game/grid.cpp
  src/game/game.cpp
  src/game/humanoid.cpp
  src/game/player.cpp
  src/game/item.cpp
)
target_include_directories(
  game
  SYSTEM
  PUBLIC
  ${CMAKE_SOURCE_DIR}/extern/eigen-3.4.0
  ${CMAKE_SOURCE_DIR}/extern/magic_enum-0.9.7/include
)
target_include_directories(
  game
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(game PUBLIC FPSPARTY_SERIAL_EIGEN)
target_link_libraries(game net)

# libscene
add_library(
  scene
  src/scene/component_frame.cpp
  src/scene/element_frame.cpp
  src/scene/scene.cpp
)
target_include_directories(
  scene
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(scene PUBLIC game)

# libclient
add_library(
  client
  src/client/application.cpp
  src/client/block_mesh/block_mesh.cpp
  src/client/block_mesh/full.cpp
  src/client/block_mod/conveyor.cpp
  src/client/block_mod/dirt.cpp
  src/client/block_mod/light.cpp
  src/client/block_mod/placeholder.cpp
  src/client/block_mod/stone.cpp
  src/client/block_model_registry.cpp
  src/client/block_texture_registry.cpp
  src/client/client.cpp
  src/client/grid_mesh.cpp
  src/client/local_player.cpp
  src/client/passes/composite_pass.cpp
  src/client/passes/crosshair_pass.cpp
  src/client/passes/distant_irradiance_passes.cpp
  src/client/passes/gbuffer_pass.cpp
  src/client/passes/radiance_pass.cpp
  src/client/passes/rt_entity_binning_pass.cpp
  src/client/passes/sky_irradiance_pass.cpp
  src/client/passes/sky_view_pass.cpp
  src/client/session.cpp
  src/client/texture_manager.cpp
)
target_include_directories(
  client
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(
  client
  SYSTEM
  PRIVATE
  ${CMAKE_SOURCE_DIR}/extern/tracy-0.13.1/public
)
target_link_libraries(
  client
  PUBLIC game graphics render_graph scene net ppm
  PRIVATE tracy
)

add_executable(
  net-tests
  tests/net/transport.cpp
)
target_link_libraries(net-tests net Catch2::Catch2WithMain)
add_test(NAME net-tests COMMAND net-tests)

add_executable(
  scene-tests
  tests/scene/scene.cpp
)
target_link_libraries(scene-tests scene Catch2::Catch2WithMain)
add_test(NAME scene-tests COMMAND scene-tests)

add_executable(
  client-tests
  tests/client/block_model_registry.cpp
  tests/client/session.cpp
)
target_link_libraries(client-tests client Catch2::Catch2WithMain)
add_test(NAME client-tests COMMAND client-tests)

add_executable(
  game-tests
  tests/game/box_tree.cpp
  tests/game/entity_world.cpp
  tests/game/grid.cpp
  tests/game/grid_raycast.cpp
)
target_link_libraries(game-tests game Catch2::Catch2WithMain)
add_test(NAME game-tests COMMAND game-tests)

add_executable(
  server-tests
  tests/server/server.cpp
  src/server/server.cpp
)
target_include_directories(
  server-tests
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(
  server-tests
  SYSTEM
  PRIVATE
  ${CMAKE_SOURCE_DIR}/extern/tracy-0.13.1/public
)
target_link_libraries(server-tests game net tracy Catch2::Catch2WithMain)
add_test(NAME server-tests COMMAND server-tests)

# libppm
add_library(
  ppm
  src/ppm/ppm.cpp
)
target_include_directories(
  ppm
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)

add_executable(
  ppm-tests
  tests/ppm/ppm.cpp
)
target_link_libraries(ppm-tests ppm Catch2::Catch2WithMain)
add_test(NAME ppm-tests COMMAND ppm-tests)

add_executable(
  render-graph-tests
  tests/render_graph.cpp
)
target_link_libraries(render-graph-tests render_graph Catch2::Catch2WithMain)
add_test(NAME render-graph-tests COMMAND render-graph-tests)

add_executable(
  int-map-tests
  tests/int_map.cpp
)
target_include_directories(
  int-map-tests
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(int-map-tests Catch2::Catch2WithMain)
add_test(NAME int-map-tests COMMAND int-map-tests)

# server
add_executable(
  server
  src/server/main.cpp
  src/server/server.cpp
)
target_include_directories(
  server
  SYSTEM
  PRIVATE
  ${CMAKE_SOURCE_DIR}/extern/enet/include
  ${CMAKE_SOURCE_DIR}/extern/tracy-0.13.1/public
)
target_include_directories(
  server
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(server enet tracy game net)

# libgraphics
add_library(
  graphics
  src/graphics/buffer.cpp
  src/graphics/compute_pipeline.cpp
  src/graphics/descriptor_heap.cpp
  src/graphics/image.cpp
  src/graphics/graphics.cpp
  src/graphics/global_vulkan_state.cpp
  src/graphics/mapped_memory.cpp
  src/graphics/pipeline.cpp
  src/graphics/shader.cpp
  src/graphics/work.cpp
  src/graphics/work_queue.cpp
  src/graphics/work_recorder.cpp
  src/graphics/work_resource.cpp
  src/graphics/work_resource_pool.cpp
  src/vma.cpp
)
target_compile_definitions(
  graphics
  PUBLIC
  GLFW_INCLUDE_VULKAN
  VK_NO_PROTOTYPES
  VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYNAMIC_VULKAN_FUNCTIONS=0
)
target_include_directories(
  graphics
  SYSTEM
  PUBLIC
  ${CMAKE_SOURCE_DIR}/extern/Vulkan-Headers/include
  ${CMAKE_SOURCE_DIR}/extern/eigen-3.4.0
)
target_include_directories(
  graphics
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(
  graphics
  SYSTEM
  PRIVATE
  ${CMAKE_SOURCE_DIR}/extern/tracy-0.13.1/public
)
target_link_libraries(graphics glfw volk GPUOpen::VulkanMemoryAllocator)
target_link_libraries(graphics spirv-reflect)

# librender_graph
add_library(
  render_graph
  src/render_graph/builder.cpp
  src/render_graph/graph.cpp
  src/render_graph/resources.cpp
  src/render_graph/schedule.cpp
)
target_include_directories(
  render_graph
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(render_graph PUBLIC graphics)

# client
add_executable(
  client-app
  src/client/main.cpp
)
target_include_directories(
  client-app
  SYSTEM
  PRIVATE
  ${CMAKE_SOURCE_DIR}/extern/enet/include
  ${CMAKE_SOURCE_DIR}/extern/magic_enum-0.9.7/include
  ${CMAKE_SOURCE_DIR}/extern/tracy-0.13.1/public
)
target_include_directories(
  client-app
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(client-app enet tracy client)

# compiler options
set(fpsparty_targets
  net
  game
  scene
  client
  net-tests
  scene-tests
  client-tests
  game-tests
  server-tests
  ppm
  ppm-tests
  int-map-tests
  render-graph-tests
  server
  graphics
  render_graph
  client-app
)
set_property(TARGET ${fpsparty_targets} PROPERTY CXX_STANDARD 23)
set_property(TARGET ${fpsparty_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${fpsparty_targets} PROPERTY CXX_EXTENSIONS OFF)
if (MSVC)
  foreach(target ${fpsparty_targets})
    target_compile_options(${target} PRIVATE /W4 /WX)
  endforeach()
else()
  set(extern_compile_options -stdlib=libc++ -O3)
  target_compile_options(tracy PRIVATE ${extern_compile_options})
  target_compile_options(spirv-reflect PRIVATE ${extern_compile_options})
  target_compile_options(Catch2 PRIVATE ${extern_compile_options})
  target_compile_options(Catch2WithMain PRIVATE ${extern_compile_options})
  set(fpsparty_compile_options -Wall -Wextra -Wpedantic -Werror -stdlib=libc++)
  foreach(target ${fpsparty_targets})
    target_compile_options(${target} PRIVATE ${fpsparty_compile_options})
  endforeach()
  set(extern_link_options -stdlib=libc++ -lc++abi)
  target_link_options(tracy PRIVATE ${extern_link_options})
  target_link_options(spirv-reflect PRIVATE ${extern_link_options})
  target_link_options(Catch2 PRIVATE ${extern_link_options})
  target_link_options(Catch2WithMain PRIVATE ${extern_link_options})
  set(fpsparty_link_options -stdlib=libc++ -lc++abi)
  foreach(target ${fpsparty_targets})
    target_link_options(${target} PRIVATE ${fpsparty_link_options})
  endforeach()
endif()
