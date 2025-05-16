cmake_minimum_required(VERSION 4.0.0)
project(gridgame VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# dependencies
add_subdirectory(extern/glfw-3.4)

# client
add_executable(
  client
  src/client/main.cpp
)
target_compile_definitions(
  client
  PRIVATE
  GLFW_INCLUDE_VULKAN
  VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
)
target_include_directories(
  client
  SYSTEM
  PRIVATE
  ${CMAKE_SOURCE_DIR}/extern/enet/include
)
target_include_directories(
  client
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_link_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/extern/enet/.libs)
target_link_libraries(client enet glfw vulkan)

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
target_link_libraries(server enet)

# compiler options
set_property(TARGET client server PROPERTY CXX_STANDARD 23)
set_property(TARGET client server PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET client server PROPERTY CXX_EXTENSIONS OFF)
if (MSVC)
  target_compile_options(client PRIVATE /W4 /WX)
  target_compile_options(server PRIVATE /W4 /WX)
else()
  target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
