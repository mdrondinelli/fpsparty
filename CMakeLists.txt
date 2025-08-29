cmake_minimum_required(VERSION 3.22.0)
project(fpsparty VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# dependencies
add_subdirectory(extern/glfw-3.4)
add_subdirectory(extern/volk-1.4.304)
add_subdirectory(extern/VulkanMemoryAllocator-3.3.0)
add_subdirectory(extern/magic_enum-0.9.7)

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

# libgame-core
add_library(
  game-core
  src/game/core/grid.cpp
  src/game/core/entity.cpp
  src/game/core/entity_world.cpp
  src/game/core/humanoid_movement.cpp
  src/game/core/projectile_movement.cpp
)
target_include_directories(
  game-core
  SYSTEM
  PUBLIC
  ${CMAKE_SOURCE_DIR}/extern/eigen-3.4.0
  ${CMAKE_SOURCE_DIR}/extern/magic_enum-0.9.7/include
)
target_include_directories(
  game-core
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(game-core PUBLIC FPSPARTY_SERIAL_EIGEN)

# libgame-server
add_library(
  game-server
  src/game/server/game.cpp
  src/game/server/humanoid.cpp
  src/game/server/player.cpp
  src/game/server/projectile.cpp
  src/game/server/server.cpp
)
target_include_directories(
  game-server
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(game-server game-core net)

# libgame-client
add_library(
  game-client
  src/game/client/client.cpp
  src/game/client/replicated_game.cpp
  src/game/client/replicated_player.cpp
  src/game/client/replicated_humanoid.cpp
  src/game/client/replicated_projectile.cpp
)
target_include_directories(
  game-client
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(game-client game-core net)

# server
add_executable(
  server
  src/server/main.cpp
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
target_link_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/extern/enet/.libs)
target_link_libraries(server enet game-server net)

# graphics
add_library(
  graphics
  src/graphics/buffer.cpp
  src/graphics/image.cpp
  src/graphics/graphics.cpp
  src/graphics/global_vulkan_state.cpp
  src/graphics/index_buffer.cpp
  src/graphics/pipeline_layout.cpp
  src/graphics/pipeline.cpp
  src/graphics/shader.cpp
  src/graphics/staging_buffer.cpp
  src/graphics/vertex_buffer.cpp
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
  ${CMAKE_SOURCE_DIR}/extern/eigen-3.4.0
)
target_include_directories(
  graphics
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(graphics glfw volk GPUOpen::VulkanMemoryAllocator)

# client
add_executable(
  client
  src/client/depth_image_recycler.cpp
  src/client/frame_counter.cpp
  src/client/main.cpp
)
target_include_directories(
  client
  SYSTEM
  PRIVATE
  ${CMAKE_SOURCE_DIR}/extern/enet/include
  ${CMAKE_SOURCE_DIR}/extern/magic_enum-0.9.7/include
)
target_include_directories(
  client
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/extern/enet/.libs)
target_link_libraries(client enet net game-client graphics)

# compiler options
set_property(TARGET net game-core game-server game-client server graphics client PROPERTY CXX_STANDARD 23)
set_property(TARGET net game-core game-server game-client server graphics client PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET net game-core game-server game-client server graphics client PROPERTY CXX_EXTENSIONS OFF)
if (MSVC)
  target_compile_options(net PRIVATE /W4 /WX)
  target_compile_options(game-core PRIVATE /W4 /WX)
  target_compile_options(game-server PRIVATE /W4 /WX)
  target_compile_options(game-client PRIVATE /W4 /WX)
  target_compile_options(server PRIVATE /W4 /WX)
  target_compile_options(graphics PRIVATE /W4 /WX)
  target_compile_options(client PRIVATE /W4 /WX)
else()
  target_compile_options(net PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(game-core PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(game-server PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(game-client PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(graphics PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_link_options(net PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(game-core PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(game-server PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(game-client PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(server PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(graphics PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(client PRIVATE -stdlib=libc++ -lc++abi)
endif()
