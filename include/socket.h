//!
//! @file socket.h
//!

#if !defined(JVS_NETLIB_SOCKET_H_)
#define JVS_NETLIB_SOCKET_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "error.h"
#include "ip_address.h"
#include "ip_end_point.h"
#include "socket_errors.h"

namespace jvs::net
{

//!
//! @class Socket
//!
//! Provides an error-checked wrapper around BSD and Winsock interfaces.
//!
class Socket final
{
public:
  enum class Transport
  {
    Tcp,
    Udp,
    Raw
  };

  Socket(Socket&&);
  ~Socket();

  Socket(net::IpAddress::Family addressFamily, Transport transport);

  // Bound/listening endpoint.
  IpEndPoint local() const noexcept;
  // Remote endpoint of the communication, if any.
  const std::optional<IpEndPoint>& remote() const noexcept;
  // Handle/context/file descriptor of the native socket resource.
  std::intptr_t descriptor() const noexcept;

  Expected<Socket> accept() noexcept;

  // Number of bytes available for reading from the socket.
  Expected<std::size_t> available() noexcept;

  Expected<IpEndPoint> bind(IpEndPoint localEndPoint) noexcept;
  Expected<IpEndPoint> bind(IpAddress localAddress, NetworkU16 localPort) noexcept;
  Expected<IpEndPoint> bind(IpAddress localAddress) noexcept;
  Expected<IpEndPoint> bind(NetworkU16 localPort) noexcept;
  Expected<IpEndPoint> bind() noexcept;

  int close() noexcept;

  Expected<IpEndPoint> connect(IpEndPoint remoteEndPoint) noexcept;
  Expected<IpEndPoint> connect(IpAddress remoteAddress, NetworkU16 remotePort) noexcept;

  Expected<IpEndPoint> listen() noexcept;
  Expected<IpEndPoint> listen(int backlog) noexcept;

  Expected<std::size_t> recv(void* buffer, std::size_t length, int flags) noexcept;
  Expected<std::size_t> recv(void* buffer, std::size_t length) noexcept;
  Expected<std::pair<std::size_t, IpEndPoint>> recvfrom(
    void* buffer, std::size_t length, int flags) noexcept;
  Expected<std::pair<std::size_t, IpEndPoint>> recvfrom(void* buffer, std::size_t length) noexcept;

  Expected<std::size_t> send(const void* buffer, std::size_t length, int flags) noexcept;
  Expected<std::size_t> send(const void* buffer, std::size_t length) noexcept;
  Expected<std::size_t> sendto(
    const void* buffer, std::size_t length, int flags, const IpEndPoint& remoteEp) noexcept;
  Expected<std::size_t> sendto(
    const void* buffer, std::size_t length, const IpEndPoint& remoteEp) noexcept;

private:
  class SocketImpl;
  std::unique_ptr<SocketImpl> impl_;

  Socket(SocketImpl* impl);
};

}  // namespace jvs::net

#endif  // !JVS_NETLIB_SOCKET_H_
