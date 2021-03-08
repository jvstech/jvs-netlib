//!
//! @file socket_impl.cpp
//! 
//! Contains private implementations of functions used in multiple translation
//! units for both the BSD and Winsock APIs. 
//! 

#include "socket_impl.h"

#include "native_sockets.h"

#include "socket_errors.h"

#include "socket_context.h"
#include "socket_types.h"
#include "utils.h"

using namespace jvs;
using namespace jvs::net;
using namespace jvs::net::errcodes;
using Family = IpAddress::Family;
using Transport = Socket::Transport;

addrinfo jvs::net::create_empty_hints() noexcept
{
  return create_zero_filled<addrinfo>();
}

addrinfo jvs::net::create_hints(
  Family addressFamily, Transport transport) noexcept
{
  auto hints = create_empty_hints();
  hints.ai_family = get_address_family(addressFamily);
  hints.ai_socktype = static_cast<int>(get_socket_transport(transport));
  hints.ai_flags = AI_PASSIVE; // let the system choose an address
  return hints;
}

Expected<SocketInfo> jvs::net::create_socket(
  Family addressFamily, Transport transport) noexcept
{
  addrinfo hints = create_hints(addressFamily, transport);
  addrinfo* ainfo;
  int status =
    ::getaddrinfo(/*nodeName*/ nullptr, /*serviceName*/ "0", &hints, &ainfo);
  if (status == 0)
  {
    SocketInfo result(*ainfo);
    ::freeaddrinfo(ainfo);
    SocketContext ctx = ::socket(get_address_family(result.family()),
      static_cast<int>(result.socket_transport()),
      static_cast<int>(result.network_transport()));
    result.set_context(ctx);
    if (!is_error_result(ctx))
    {
      return result;
    }
    else
    {
      return create_socket_error(get_last_error());
    }
  }
  else
  {
    return create_addrinfo_error(status);
  }
}

Error jvs::net::create_socket_error(int ecode) noexcept
{
  if (!ecode)
  {
    return Error::success();
  }

  if (ecode == EAgain || ecode == EWouldBlock || ecode == EInProgress)
  {
    return jvs::make_error<NonBlockingStatus>(ecode);
  }

  if (ecode == EOpNotSupp || ecode == EAFNoSupport || ecode == EPFNoSupport ||
    ecode == EProtoNoSupport || ecode == ESockTNoSupport)
  {
    return jvs::make_error<UnsupportedError>(ecode);
  }

  return jvs::make_error<SocketError>(ecode);
}

Error jvs::net::create_socket_error(SocketContext s) noexcept
{
  int ecode{};
  socklen_t codeSize = static_cast<socklen_t>(sizeof(ecode));
  int lastErr = get_last_error();
  auto result = ::getsockopt(
    s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&ecode), &codeSize);
  if (is_error_result(result) || (lastErr != 0 && ecode == 0))
  {
    return create_socket_error(lastErr);
  }

  return create_socket_error(ecode);
}

Error jvs::net::create_addrinfo_error(int ecode) noexcept
{
  return jvs::make_error<AddressInfoError>(ecode);
}
