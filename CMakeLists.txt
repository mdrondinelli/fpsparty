cmake_minimum_required(VERSION 3.22.0)
project(fpsparty VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(TRACY_ENABLE)

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
  extern/tracy-0.12.2/TracyClient.cpp
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
  src/game/grid.cpp
  src/game/humanoid_movement.cpp
  src/game/projectile_movement.cpp
  src/game/game.cpp
  src/game/humanoid.cpp
  src/game/player.cpp
  src/game/projectile.cpp
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
  src/client/client.cpp
  src/client/session.cpp
  src/client/grid_mesh.cpp
  src/client/local_player.cpp
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
  ${CMAKE_SOURCE_DIR}/extern/tracy-0.12.2
)
target_link_libraries(
  client
  PUBLIC game graphics scene net ppm
  PRIVATE tracy
)

add_executable(
  net-tests
  tests/net/transport.cpp
)
target_link_libraries(net-tests net Catch2::Catch2WithMain)
add_test(NAME net-tests COMMAND net-tests)

add_executable(
  game-tests
  tests/game/entity_world.cpp
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
target_link_libraries(server-tests game net Catch2::Catch2WithMain)
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
)
target_include_directories(
  server
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(server enet game net)

# libgraphics
add_library(
  graphics
  src/graphics/buffer.cpp
  src/graphics/descriptor_heap_pool.cpp
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
  ${CMAKE_SOURCE_DIR}/extern/tracy-0.12.2
)
target_link_libraries(graphics glfw volk GPUOpen::VulkanMemoryAllocator)

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
  ${CMAKE_SOURCE_DIR}/extern/tracy-0.12.2
)
target_include_directories(
  client-app
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(client-app enet tracy client)

# compiler options
set_property(TARGET net game scene client net-tests game-tests server-tests ppm ppm-tests server graphics client-app PROPERTY CXX_STANDARD 23)
set_property(TARGET net game scene client net-tests game-tests server-tests ppm ppm-tests server graphics client-app PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET net game scene client net-tests game-tests server-tests ppm ppm-tests server graphics client-app PROPERTY CXX_EXTENSIONS OFF)
if (MSVC)
  target_compile_options(net PRIVATE /W4 /WX)
  target_compile_options(game PRIVATE /W4 /WX)
  target_compile_options(scene PRIVATE /W4 /WX)
  target_compile_options(client PRIVATE /W4 /WX)
  target_compile_options(net-tests PRIVATE /W4 /WX)
  target_compile_options(game-tests PRIVATE /W4 /WX)
  target_compile_options(server-tests PRIVATE /W4 /WX)
  target_compile_options(ppm PRIVATE /W4 /WX)
  target_compile_options(ppm-tests PRIVATE /W4 /WX)
  target_compile_options(server PRIVATE /W4 /WX)
  target_compile_options(graphics PRIVATE /W4 /WX)
  target_compile_options(client-app PRIVATE /W4 /WX)
else()
  set(extern_compile_options -stdlib=libc++ -O3)
  target_compile_options(tracy PRIVATE ${extern_compile_options})
  target_compile_options(Catch2 PRIVATE ${extern_compile_options})
  target_compile_options(Catch2WithMain PRIVATE ${extern_compile_options})
  set(fpsparty_compile_options -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(net PRIVATE ${fpsparty_compile_options})
  target_compile_options(game PRIVATE ${fpsparty_compile_options})
  target_compile_options(scene PRIVATE ${fpsparty_compile_options})
  target_compile_options(client PRIVATE ${fpsparty_compile_options})
  target_compile_options(net-tests PRIVATE ${fpsparty_compile_options})
  target_compile_options(game-tests PRIVATE ${fpsparty_compile_options})
  target_compile_options(server-tests PRIVATE ${fpsparty_compile_options})
  target_compile_options(ppm PRIVATE ${fpsparty_compile_options})
  target_compile_options(ppm-tests PRIVATE ${fpsparty_compile_options})
  target_compile_options(server PRIVATE ${fpsparty_compile_options})
  target_compile_options(graphics PRIVATE ${fpsparty_compile_options})
  target_compile_options(client-app PRIVATE ${fpsparty_compile_options})
  set(extern_link_options -stdlib=libc++ -lc++abi)
  target_link_options(tracy PRIVATE ${extern_link_options})
  target_link_options(Catch2 PRIVATE ${extern_link_options})
  target_link_options(Catch2WithMain PRIVATE ${extern_link_options})
  set(fpsparty_link_options -stdlib=libc++ -lc++abi)
  target_link_options(net PRIVATE ${fpsparty_link_options})
  target_link_options(game PRIVATE ${fpsparty_link_options})
  target_link_options(scene PRIVATE ${fpsparty_link_options})
  target_link_options(client PRIVATE ${fpsparty_link_options})
  target_link_options(net-tests PRIVATE ${fpsparty_link_options})
  target_link_options(game-tests PRIVATE ${fpsparty_link_options})
  target_link_options(server-tests PRIVATE ${fpsparty_link_options})
  target_link_options(ppm PRIVATE ${fpsparty_link_options})
  target_link_options(ppm-tests PRIVATE ${fpsparty_link_options})
  target_link_options(server PRIVATE ${fpsparty_link_options})
  target_link_options(graphics PRIVATE ${fpsparty_link_options})
  target_link_options(client-app PRIVATE ${fpsparty_link_options})
endif()
