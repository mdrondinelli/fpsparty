#ifndef FPSPARTY_SERIAL_WRITE_ERROR_HPP
#define FPSPARTY_SERIAL_WRITE_ERROR_HPP

#include <exception>
namespace fpsparty::serial {
class Write_error : public std::exception {};
} // namespace fpsparty::serial

#endif
