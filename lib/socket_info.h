#if !defined(JVS_NETLIB_SOCKET_INFO_H_)
#define JVS_NETLIB_SOCKET_INFO_H_

#include "native_sockets.h"

#include "ip_address.h"
#include "network_integers.h"
#include "socket.h"

#include "socket_types.h"
#include "utils.h"

namespace jvs::net
{

//!
//! @struct SocketInfo
//!
//! Configuration-like class for storing and caching address and transport
//! information about a socket.
//!
class SocketInfo final
{
public:
  SocketInfo() = default;

  SocketInfo(const IpAddress& ipAddress);
  SocketInfo(const addrinfo& addrInfo);

  explicit SocketInfo(SocketContext ctx);

  const IpAddress& address() const noexcept;
  
  IpAddress::Family family() const noexcept;
  
  SocketInfo& set_family(IpAddress::Family family) noexcept;

  Socket::Transport transport() const noexcept;

  SocketInfo& set_transport(Socket::Transport transport) noexcept;

  NetworkTransport network_transport() const noexcept;

  SocketInfo& set_network_transport(NetworkTransport transport) noexcept;

  SocketTransport socket_transport() const noexcept;

  SocketInfo& set_socket_transport(SocketTransport transport) noexcept;

  // Sets all transport types to the defaults mapped from the given well-defined
  // transport type.
  SocketInfo& set_transports(Socket::Transport transport) noexcept;

  // Sets all transport types to the defaults mapped from the given network
  // transport type.
  SocketInfo& set_transports(NetworkTransport transport) noexcept;

  // Sets all transport types to the defaults mapped from the given socket
  // transport type.
  SocketInfo& set_transports(SocketTransport transport) noexcept;

  SocketContext context() const noexcept;

  SocketInfo& set_context(SocketContext ctx) noexcept;

  NetworkU16 port() const noexcept;

  SocketInfo& set_port(NetworkU16 port) noexcept;

  void reset() noexcept;

private:
  IpAddress address_;
  IpAddress::Family family_{IpAddress::Family::Unspecified};
  Socket::Transport transport_{Socket::Transport::Raw};
  NetworkTransport network_transport_{NetworkTransport::Unspecified};
  SocketTransport socket_transport_{SocketTransport::Unspecified};
  SocketContext context_{static_cast<SocketContext>(-1)};
  jvs::net::NetworkU16 port_{0};

  friend jvs::Expected<SocketInfo> create_socket(
    IpAddress::Family addressFamily, Socket::Transport transport) noexcept;
};


} // namespace jvs::net


#endif // !JVS_NETLIB_SOCKET_INFO_H_
