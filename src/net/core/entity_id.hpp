#ifndef FPSPARTY_NET_CORE_ENTITY_ID_HPP
#define FPSPARTY_NET_CORE_ENTITY_ID_HPP

#include <cstdint>

namespace fpsparty::net {

// 0 is reserved to represent no entity.
using Entity_id = std::uint32_t;

}

#endif
