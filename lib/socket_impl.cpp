//!
//! @file socket_impl.cpp
//! 
//! Contains private implementations of functions used in multiple translation
//! units for both the BSD and Winsock APIs. 
//! 

#include "socket_impl.h"

#include "native_socket_includes.h"

#include "socket_types.h"
#include "utils.h"

using namespace jvs;
using namespace jvs::net;
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