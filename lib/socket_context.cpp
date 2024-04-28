#include <jvs-netlib/socket_context.h>

jvs::net::SocketContext::SocketContext(type socketHandleOrDescriptor)
  : value_(socketHandleOrDescriptor)
{
}

jvs::net::SocketContext::operator type() const noexcept
{
  return value_;
}

jvs::net::SocketContext::type jvs::net::SocketContext::value() const noexcept
{
  return value_;
}
