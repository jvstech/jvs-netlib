#include "bsd_sockets_impl.h"

#include <sys/ioctl.h>
#include "native_sockets.h"

#include "socket.h"
#include "socket_errors.h"

#include "socket_impl.h"
#include "utils.h"

using namespace jvs::net;
using namespace jvs::net::errcodes;
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

int jvs::net::get_last_error() noexcept
{
  return errno;
}

std::string jvs::net::get_socket_error_message(int ecode) noexcept
{
  return strerror(ecode);
}

std::string jvs::net::get_addrinfo_error_message(int ecode) noexcept
{
  return gai_strerror(ecode);
}

// BSD API-specific Socket member implementations
////////////////////////////////////////////////////////////////////////////////

jvs::Expected<std::size_t> Socket::available() noexcept
{
  auto ctx = impl_->socket_info_.context();
  int err = 0;
  int bytesAvailable = 0;
  while ((err = ioctl(ctx, FIONREAD, &bytesAvailable)) < 0 &&
    get_last_error() == EINTR)
  {
  }

  if (is_error_result(err))
  {
    return jvs::net::create_socket_error(err);
  }

  return static_cast<std::size_t>(bytesAvailable);
}

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
