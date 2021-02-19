#if !defined(JVS_NETLIB_SOCKET_IMPL_H_)
#define JVS_NETLIB_SOCKET_IMPL_H_

#include <cstddef>
#include <optional>
#include <utility>

#include "bsd_socket_info.h"
#include "bsd_sockets.h"
#include "error.h"
#include "network_integers.h"
#include "socket.h"

namespace jvs::net
{

class Socket::SocketImpl
{
  bsd::SocketInfo socket_info_{};
  std::optional<IpEndPoint> local_endpoint_{};
  std::optional<IpEndPoint> remote_endpoint_{};

public:
  SocketImpl(IpAddress::Family addressFamily, Socket::Transport transport);
  SocketImpl(Socket::Transport transport);
  SocketImpl(int sockfd);

  IpEndPoint local() const noexcept
  {
    if (local_endpoint_)
    {
      return *local_endpoint_;
    }

    return IpEndPoint(socket_info_.address(), socket_info_.port());
  }

  const std::optional<IpEndPoint>& remote() const noexcept
  {
    return remote_endpoint_;
  }

  Expected<IpEndPoint> bind(IpEndPoint localEndPoint) noexcept;
  
  bool update_local_endpoint() noexcept;
  
  bool update_remote_endpoint() noexcept;
  
  //!
  //! Accepts a connection
  //!
  //! @returns a new Socket for communicating with the remote endpoint of the
  //!          accepted connection or a socket error if the connection failed.
  //!
  Expected<Socket> accept() noexcept;
  
  Expected<IpEndPoint> bind(IpAddress address, NetworkU16 port) noexcept;
  
  Expected<IpEndPoint> bind(IpAddress localAddress) noexcept;
  
  Expected<IpEndPoint> bind(NetworkU16 port) noexcept;
  
  Expected<IpEndPoint> bind() noexcept;
  
  int close() noexcept;
  
  Expected<IpEndPoint> connect(IpEndPoint remoteEndPoint) noexcept;
  
  Expected<IpEndPoint> listen(int backlog) noexcept;
  
  Expected<std::size_t> recv(
    void* buffer, std::size_t length, int flags) noexcept;
  
  Expected<std::pair<std::size_t, IpEndPoint>> recvfrom(
    void* buffer, std::size_t length, int flags) noexcept;
  
  Expected<std::size_t> send(
    const void* buffer, std::size_t length, int flags) noexcept;
  
  Expected<std::size_t> sendto(const void* buffer, std::size_t length,
    int flags, const IpEndPoint& remoteEp) noexcept;
};

} // namespace jvs::net

#endif // !JVS_NETLIB_SOCKET_IMPL_H_