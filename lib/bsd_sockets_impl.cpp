#include "bsd_sockets_impl.h"

#include "native_socket_includes.h"

#include "socket.h"

#include "socket_impl.h"
#include "utils.h"

using namespace jvs::net;
using Family = IpAddress::Family;
using Transport = Socket::Transport;

Socket::SocketImpl::SocketImpl() = default;

Socket::SocketImpl::SocketImpl(SocketContext ctx)
  : socket_info_(ctx)
{
}

Socket::SocketImpl::SocketImpl(Family addressFamily, Transport transport)
{
  if (auto si = create_socket(addressFamily, transport))
  {
    socket_info_ = *si;
  }
  else
  {
    jvs::consume_error(si.take_error());
  }
}

Socket::SocketImpl::~SocketImpl() = default;

const std::optional<IpEndPoint>& Socket::SocketImpl::local_endpoint()
  const noexcept
{
  return local_endpoint_;
}

const std::optional<IpEndPoint>& Socket::SocketImpl::remote_endpoint()
  const noexcept
{
  return remote_endpoint_;
}

jvs::Error jvs::net::create_socket_error(int ecode) noexcept
{
  if (ecode == EAGAIN || ecode == EWOULDBLOCK)
  {
    return jvs::make_error<NonBlockingStatus>();
  }

  return jvs::make_error<SocketError>(ecode, strerror(ecode));
}

jvs::Error jvs::net::create_addrinfo_error(int ecode) noexcept
{
  return jvs::make_error<SocketError>(ecode, gai_strerror(ecode));
}

int jvs::net::get_last_error() noexcept
{
  return errno;
}

// BSD API-specific Socket member implementations
////////////////////////////////////////////////////////////////////////////////

// See winsock_impl.cpp as to why this can't be implemented as a common
// function.
int Socket::close() noexcept
{
  int closeResult = ::close(impl_->socket_info_.context());
  impl_->socket_info_.reset();
  impl_->local_endpoint_.reset();
  impl_->remote_endpoint_.reset();
  return closeResult;
}
