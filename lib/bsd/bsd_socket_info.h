#if !defined(JVS_NETLIB_SOCKET_INFO_H_)
#define JVS_NETLIB_SOCKET_INFO_H_

#include <netdb.h>

#include "bsd_sockets.h"

namespace bsd
{

//!
//! @struct SocketInfo
//!
//! Contains address and transport information about a socket.
//!
struct SocketInfo
{
  SocketInfo() = default;

  SocketInfo(const IpAddress& ipAddress)
    : address_(ipAddress),
    family_(address_.family())
  {
  }

  SocketInfo(const addrinfo& addrInfo)
    : address_(get_ip_address(addrInfo)),
    family_(get_address_family(addrInfo.ai_family))
  {
    set_transports(static_cast<SocketTransport>(addrInfo.ai_socktype));
  }

  explicit SocketInfo(SocketHandle sockfd)
    : descriptor_(sockfd)
  {
    if (auto localEndpoint = get_local_endpoint(sockfd))
    {
      address_ = localEndpoint->address();
      family_ = address_.family();
    }
  }

  const IpAddress& address() const noexcept
  {
    return address_;
  }

  Family family() const noexcept
  {
    return family_;
  }

  SocketInfo& set_family(Family family) noexcept
  {
    family_ = family;
    return *this;
  }

  Transport transport() const noexcept
  {
    return transport_;
  }

  SocketInfo& set_transport(Transport transport) noexcept
  {
    transport_ = transport;
    network_transport_ = get_network_transport(transport_);
    socket_transport_ = get_socket_transport(transport_);
    return *this;
  }

  NetworkTransport network_transport() const noexcept
  {
    return network_transport_;
  }

  SocketInfo& set_network_transport(NetworkTransport transport) noexcept
  {
    network_transport_ = transport;
    return *this;
  }

  SocketTransport socket_transport() const noexcept
  {
    return socket_transport_;
  }

  SocketInfo& set_socket_transport(SocketTransport transport) noexcept
  {
    socket_transport_ = transport;
    return *this;
  }

  // Sets all transport types to the defaults mapped from the given well-defined
  // transport type.
  SocketInfo& set_transports(Transport transport) noexcept
  {
    transport_ = transport;
    network_transport_ = get_network_transport(transport_);
    socket_transport_ = get_socket_transport(transport_);
    return *this;
  }

  // Sets all transport types to the defaults mapped from the given network
  // transport type.
  SocketInfo& set_transports(NetworkTransport transport) noexcept
  {
    network_transport_ = transport;
    transport_ = get_transport(network_transport_);
    socket_transport_ = get_socket_transport(network_transport_);
    return *this;
  }

  // Sets all transport types to the defaults mapped from the given socket
  // transport type.
  SocketInfo& set_transports(SocketTransport transport) noexcept
  {
    socket_transport_ = transport;
    transport_ = get_transport(socket_transport_);
    network_transport_ = get_network_transport(socket_transport_);
    return *this;
  }

  SocketHandle descriptor() const noexcept
  {
    return descriptor_;
  }

  SocketInfo& set_descriptor(SocketHandle descriptor) noexcept
  {
    descriptor_ = descriptor;
    return *this;
  }

  jvs::net::NetworkU16 port() const noexcept
  {
    return port_;
  }

  SocketInfo& set_port(jvs::net::NetworkU16 port) noexcept
  {
    port_ = port;
    return *this;
  }

  void reset() noexcept
  {
    address_ = IpAddress::ipv4_any();
    family_ = Family::Unspecified;
    set_transports(NetworkTransport::Unspecified);
    transport_ = Transport::Raw;
    descriptor_ = -1;
    port_ = 0;
  }

private:
  IpAddress address_;
  Family family_{Family::Unspecified};
  Transport transport_{Transport::Raw};
  NetworkTransport network_transport_{NetworkTransport::Unspecified};
  SocketTransport socket_transport_{SocketTransport::Unspecified};
  SocketHandle descriptor_{-1};
  jvs::net::NetworkU16 port_{0};

  friend jvs::Expected<SocketInfo> create_socket(Transport transport) noexcept;
  friend jvs::Expected<SocketInfo> create_socket(
    Family addressFamily, Transport transport) noexcept;
};


} // namespace bsd

#endif // !JVS_NETLIB_SOCKET_INFO_H_
