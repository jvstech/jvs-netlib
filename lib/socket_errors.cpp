#include <jvs-netlib/socket_errors.h>

#include "socket_impl.h"

char jvs::net::SocketError::ID = 0;

jvs::net::SocketError::SocketError(int code, const std::string& message)
  : code_(code),
  message_(message)
{
}

jvs::net::SocketError::SocketError(int code)
  : SocketError(code, get_socket_error_message(code))
{
}

int jvs::net::SocketError::code() const noexcept
{
  return code_;
}

bool jvs::net::SocketError::is_fatal() const
{
  return true;
}

void jvs::net::SocketError::log(std::ostream& os) const
{
  os << message_ << " (" << code_ << std::hex << " = 0x" << code_ << std::dec
    << ")";
}

char jvs::net::SocketErrorNonFatal::ID = 0;

bool jvs::net::SocketErrorNonFatal::is_fatal() const
{
  return false;
}

char jvs::net::AddressInfoError::ID = 0;

jvs::net::AddressInfoError::AddressInfoError(int code)
  : Base(code, get_addrinfo_error_message(code))
{
}

char jvs::net::NonBlockingStatus::ID = 0;

char jvs::net::UnsupportedError::ID = 0;

