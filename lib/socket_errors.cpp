#include "socket.h"

char jvs::net::SocketError::ID = 0;

jvs::net::SocketError::SocketError(int code, const std::string& message)
  : code_(code),
  message_(message)
{
}

int jvs::net::SocketError::code() const noexcept
{
  return code_;
}

void jvs::net::SocketError::log(std::ostream& os) const
{
  os << message_ << " (" << code_ << std::hex << " = 0x" << code_ << std::dec
    << ")";
}

char jvs::net::NonBlockingStatus::ID = 0;

jvs::net::NonBlockingStatus::NonBlockingStatus() = default;

void jvs::net::NonBlockingStatus::log(std::ostream& os) const
{
  os << "Socket is non-blocking, and the operation would block.";
}

char jvs::net::UnsupportedOperationError::ID = 0;

jvs::net::UnsupportedOperationError::UnsupportedOperationError() = default;

void jvs::net::UnsupportedOperationError::log(std::ostream& os) const
{
  os << "Unsupported operation.";
}
