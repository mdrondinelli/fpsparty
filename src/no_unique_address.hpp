#ifndef FPSPARTY_NO_UNIQUE_ADDRESS_HPP
#define FPSPARTY_NO_UNIQUE_ADDRESS_HPP

// Lets an empty member occupy no space. MSVC ignores the standard
// spelling to keep its ABI stable and provides its own, so both are
// needed. If a compiler honours neither, the cost is the padding of one
// empty member -- size, never correctness.
#ifdef _MSC_VER
#define FPSPARTY_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define FPSPARTY_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

#endif
