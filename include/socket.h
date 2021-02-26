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
  Socket(Socket&&);
  ~Socket();

  enum class Transport
  {
    Tcp,
    Udp,
    Raw
  };

  Socket(net::IpAddress::Family addressFamily, Transport transport);

  IpEndPoint local() const noexcept;
  const std::optional<IpEndPoint>& remote() const noexcept;
  std::intptr_t descriptor() const noexcept;

  Expected<Socket> accept() noexcept;

  Expected<IpEndPoint> bind(IpEndPoint localEndPoint) noexcept;
  Expected<IpEndPoint> bind(IpAddress localAddress, NetworkU16 localPort) 
    noexcept;
  Expected<IpEndPoint> bind(IpAddress localAddress) noexcept;
  Expected<IpEndPoint> bind(NetworkU16 localPort) noexcept;
  Expected<IpEndPoint> bind() noexcept;

  int close() noexcept;

  Expected<IpEndPoint> connect(IpEndPoint remoteEndPoint) noexcept;
  Expected<IpEndPoint> connect(IpAddress remoteAddress, NetworkU16 remotePort)
    noexcept;

  Expected<IpEndPoint> listen() noexcept;
  Expected<IpEndPoint> listen(int backlog) noexcept;

  Expected<std::size_t> recv(void* buffer, std::size_t length, int flags)
    noexcept;
  Expected<std::size_t> recv(void* buffer, std::size_t length) noexcept;
  Expected<std::pair<std::size_t, IpEndPoint>> recvfrom(void* buffer,
    std::size_t length, int flags) noexcept;
  Expected<std::pair<std::size_t, IpEndPoint>> recvfrom(
    void* buffer, std::size_t length) noexcept;

  Expected<std::size_t> send(
    const void* buffer, std::size_t length, int flags) noexcept;
  Expected<std::size_t> send(const void* buffer, std::size_t length) noexcept;
  Expected<std::size_t> sendto(const void* buffer, std::size_t length,
    int flags, const IpEndPoint& remoteEp) noexcept;
  Expected<std::size_t> sendto(const void* buffer, std::size_t length,
    const IpEndPoint& remoteEp) noexcept;

private:
  class SocketImpl;
  std::unique_ptr<SocketImpl> impl_;

  Socket(SocketImpl* impl);
};

class SocketError : public jvs::ErrorInfo<SocketError>
{
  int code_;
  std::string message_;
public:

  static char ID;

  SocketError(int code, const std::string& message);

  int code() const noexcept;

  void log(std::ostream& os) const override;
};

class NonBlockingStatus : public jvs::ErrorInfo<NonBlockingStatus>
{
public:
  static char ID;
  NonBlockingStatus();
  void log(std::ostream& os) const override;
};

class UnsupportedOperationError : public ErrorInfo<UnsupportedOperationError>
{
public:
  static char ID;
  UnsupportedOperationError();
  void log(std::ostream& os) const override;
};

} // namespace jvs::net


#endif // !JVS_NETLIB_SOCKET_H_
