cmake_minimum_required(VERSION 3.22.0)
project(fpsparty VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# dependencies
add_subdirectory(extern/glfw-3.4)
add_subdirectory(extern/volk-1.4.304)
add_subdirectory(extern/VulkanMemoryAllocator-3.3.0)
add_subdirectory(extern/magic_enum-0.9.7)

# game_core
add_library(
  game_core
  src/game_core/humanoid_movement.cpp
  src/game_core/projectile_movement.cpp
)
target_include_directories(
  game_core
  SYSTEM
  PUBLIC
  ${CMAKE_SOURCE_DIR}/extern/eigen-3.4.0
  ${CMAKE_SOURCE_DIR}/extern/magic_enum-0.9.7/include
)
target_include_directories(
  game_core
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

# game_authority
add_library(
  game_authority
  src/game_authority/player.cpp
  src/game_authority/projectile.cpp
  src/game_authority/game.cpp
)
target_include_directories(
  game_authority
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(game_authority game_core)

# game_replica
add_library(
  game_replica
  src/game_replica/game.cpp
)
target_include_directories(
  game_replica
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(game_replica game_core)

# net
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
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

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
target_link_libraries(server enet game_authority net)

# client
add_executable(
  client
  src/vma.cpp
  src/client/global_vulkan_state.cpp
  src/client/staging_buffer.cpp
  src/client/vertex_buffer.cpp
  src/client/index_buffer.cpp
  src/client/graphics.cpp
  src/client/main.cpp
)
target_compile_definitions(
  client
  PRIVATE
  GLFW_INCLUDE_VULKAN
  VK_NO_PROTOTYPES
  VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYNAMIC_VULKAN_FUNCTIONS=0
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
target_link_libraries(client enet glfw volk GPUOpen::VulkanMemoryAllocator game_replica net)

# compiler options
set_property(TARGET game_core game_authority game_replica net server client PROPERTY CXX_STANDARD 23)
set_property(TARGET game_core game_authority game_replica net server client PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET game_core game_authority game_replica net server client PROPERTY CXX_EXTENSIONS OFF)
if (MSVC)
  target_compile_options(game_core PRIVATE /W4 /WX)
  target_compile_options(game_authority PRIVATE /W4 /WX)
  target_compile_options(game_replica PRIVATE /W4 /WX)
  target_compile_options(net PRIVATE /W4 /WX)
  target_compile_options(server PRIVATE /W4 /WX)
  target_compile_options(client PRIVATE /W4 /WX)
else()
  target_compile_options(game_core PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(game_authority PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(game_replica PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(net PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -Werror -stdlib=libc++ -O3)
  target_link_options(game_core PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(game_authority PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(game_replica PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(net PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(client PRIVATE -stdlib=libc++ -lc++abi)
  target_link_options(server PRIVATE -stdlib=libc++ -lc++abi)
endif()
