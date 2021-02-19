#if !defined(JVS_NETLIB_BSD_BSD_SOCKETS_H_)
#define JVS_NETLIB_BSD_BSD_SOCKETS_H_

#include <sys/socket.h>
#include <netinet/in.h>

#include "error.h"
#include "ip_address.h"
#include "ip_end_point.h"
#include "socket.h"

#include "bsd_socket_info.h"
#include "utils.h"

namespace bsd
{

struct SocketInfo;

using jvs::net::IpAddress;
using jvs::net::IpEndPoint;
using jvs::net::Socket;
using Family = IpAddress::Family;
using Transport = Socket::Transport;

using SocketHandle = jvs::ReturnType<decltype(::socket)>;

// Wrapper for network layer transport types in netinet/in.h
enum class NetworkTransport
{
  Unspecified,
  Tcp = IPPROTO_TCP,
  Udp = IPPROTO_UDP,
  Raw = IPPROTO_RAW
};

// Wrapper for transport layer PDU constants in enum socket_type
enum class SocketTransport : std::underlying_type_t<decltype(SOCK_STREAM)>
{
  Unspecified,
  Stream = SOCK_STREAM,
  Datagram = SOCK_DGRAM,
  Raw = SOCK_RAW
};

// Returns the default well-defined transport type for the given network 
// transport type.
static constexpr Socket::Transport get_transport(
  NetworkTransport transport) noexcept
{
  switch (transport)
  {
  case NetworkTransport::Tcp:
  case NetworkTransport::Unspecified: // Unspecified maps to TCP
    return Socket::Transport::Tcp;
  case NetworkTransport::Udp:
    return Socket::Transport::Udp;
  case NetworkTransport::Raw:
    return Socket::Transport::Raw;
  }
}

// Returns the default well-defined transport for the given socket transport 
// type.
static constexpr Socket::Transport get_transport(
  SocketTransport transport) noexcept
{
  switch (transport)
  {
  case SocketTransport::Stream:
  case SocketTransport::Unspecified: // Unspecified maps to TCP
    return Socket::Transport::Tcp;
  case SocketTransport::Datagram:
    return Socket::Transport::Udp;
  case SocketTransport::Raw:
    return Socket::Transport::Raw;
  }
}

// Returns the default network transport type for the given well-defined 
// transport type.
static constexpr NetworkTransport get_network_transport(
  Socket::Transport transport) noexcept
{
  switch (transport)
  {
  case Socket::Transport::Tcp:
    return NetworkTransport::Tcp;
  case Socket::Transport::Udp:
    return NetworkTransport::Udp;
  case Socket::Transport::Raw:
    return NetworkTransport::Raw;
  }
}

// Returns the default network transport type for the given socket transport 
// type.
static constexpr NetworkTransport get_network_transport(
  SocketTransport transport) noexcept
{
  switch (transport)
  {
  case SocketTransport::Stream:
    return NetworkTransport::Tcp;
  case SocketTransport::Datagram:
    return NetworkTransport::Udp;
  case SocketTransport::Raw:
    return NetworkTransport::Raw;
  case SocketTransport::Unspecified:
    return NetworkTransport::Unspecified;
  }
}

// Returns the default socket transport type for the given well-defined 
// transport type.
static constexpr SocketTransport get_socket_transport(
  Socket::Transport transport) noexcept
{
  switch (transport)
  {
  case Socket::Transport::Tcp:
    return SocketTransport::Stream;
  case Socket::Transport::Udp:
    return SocketTransport::Datagram;
  case Socket::Transport::Raw:
    return SocketTransport::Raw;
  }
}

// Returns the default socket transport type from the given network transport 
// type.
static constexpr SocketTransport get_socket_transport(
  NetworkTransport transport) noexcept
{
  switch (transport)
  {
  case NetworkTransport::Tcp:
  case NetworkTransport::Unspecified: // Unspecified maps to TCP
    return SocketTransport::Stream;
  case NetworkTransport::Udp:
    return SocketTransport::Datagram;
  case NetworkTransport::Raw:
    return SocketTransport::Raw;
  }
}

static constexpr int get_socket_address_family(
  IpAddress::Family addressFamily) noexcept
{
  switch (addressFamily)
  {
  case Family::IPv4:
    return PF_INET;
  case Family::IPv6:
    return PF_INET6;
  case Family::Unspecified:
    return PF_UNSPEC;
  }
}

// Given that AF_ and PF_ are almost guaranteed to be equal to each other, this
// is likely a clone of get_socket_address_family.
static constexpr int get_address_family(
  IpAddress::Family addressFamily) noexcept
{
  switch (addressFamily)
  {
  case Family::IPv4:
    return AF_INET;
  case Family::IPv6:
    return AF_INET6;
  case Family::Unspecified:
    return AF_UNSPEC;
  }
}

static constexpr IpAddress::Family get_address_family(
  int addressFamily) noexcept
{
  // Can't use switch statements here as AF_INET* and PF_INET* are almost
  // guaranteed to be the same value.

  if (addressFamily == AF_INET || addressFamily == PF_INET)
  {
    return IpAddress::Family::IPv4;
  }
  else if (addressFamily == AF_INET6 || addressFamily == PF_INET6)
  {
    return IpAddress::Family::IPv6;
  }

  return IpAddress::Family::Unspecified;
}

static constexpr socklen_t get_address_length(
  IpAddress::Family addressFamily) noexcept
{
  if (addressFamily == IpAddress::Family::IPv4)
  {
    return sizeof(::sockaddr_in);
  }

  if (addressFamily == IpAddress::Family::IPv6)
  {
    return sizeof(::sockaddr_in6);
  }

  jvs_unreachable("Unsupported address family for length.");
}

static constexpr socklen_t get_address_length(const IpAddress& addr) noexcept
{
  return get_address_length(addr.family());
}

static constexpr socklen_t get_address_length(
  const jvs::net::IpEndPoint& ep) noexcept
{
  return get_address_length(ep.address().family());
}


// Declarations

addrinfo create_empty_hints() noexcept;
addrinfo create_hints(Family addressFamily, Transport transport) noexcept;
IpAddress get_ip_address(const addrinfo& addrInfo) noexcept;
jvs::Error create_addrinfo_error(int ecode) noexcept;
jvs::Error create_socket_error(int ecode) noexcept;
jvs::Expected<IpEndPoint> get_local_endpoint(SocketHandle sockfd) noexcept;
jvs::Expected<IpEndPoint> get_remote_endpoint(SocketHandle sockfd) noexcept;
jvs::Expected<SocketInfo> create_socket(
  Family addressFamily, Transport transport) noexcept;
jvs::Expected<SocketInfo> create_socket(Transport transport) noexcept;

} // namespace bsd

// jvs namespace injection for ConvertCast
namespace jvs
{

template <>
struct ConvertCast<net::IpEndPoint, sockaddr>
{
  sockaddr_storage operator()(const net::IpEndPoint& ep) const noexcept;
};

template <>
struct ConvertCast<sockaddr, jvs::net::IpEndPoint>
{
  Expected<net::IpEndPoint> operator()(const sockaddr& addr) const noexcept;
};

template <>
struct ConvertCast<sockaddr_storage, net::IpEndPoint>
{
  Expected<net::IpEndPoint> operator()(
    const sockaddr_storage& addr) const noexcept;
};

} // namespace jvs

#endif // !JVS_NETLIB_BSD_BSD_SOCKETS_H_
