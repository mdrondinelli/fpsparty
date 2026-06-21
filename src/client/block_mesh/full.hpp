#ifndef FPSPARTY_CLIENT_BLOCK_MESH_FULL_HPP
#define FPSPARTY_CLIENT_BLOCK_MESH_FULL_HPP

#include <bitset>

#include <int.hpp>

#include "block_mesh.hpp"

namespace fpsparty::client {

Block_mesh make_full_block_mesh(u32 texture_index);

static std::bitset<6> constexpr full_block_occlusion_flags{0b111111};

}

#endif
