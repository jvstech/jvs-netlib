///
/// @file bsd_sockets_impl.h
/// 
/// Contains the declarations required for the BSD socket API wrapper
/// implementation of Socket::SocketImpl.
/// 

#if !defined(JVS_NETLIB_BSD_SOCKETS_IMPL_H_)
#define JVS_NETLIB_BSD_SOCKETS_IMPL_H_

#include <optional>

#include <jvs-netlib/ip_end_point.h>
#include <jvs-netlib/socket.h>

#include "socket_info.h"

namespace jvs::net
{

class Socket::SocketImpl final
{
public:
  SocketImpl();
  SocketImpl(SocketContext ctx);
  SocketImpl(IpAddress::Family addressFamily, Socket::Transport transport);  
  ~SocketImpl();

  const std::optional<IpEndPoint>& local_endpoint() const noexcept;
  const std::optional<IpEndPoint>& remote_endpoint() const noexcept;

  bool update_local_endpoint() noexcept;
  bool update_remote_endpoint() noexcept;

private:
  friend class Socket;

  SocketInfo socket_info_{};
  std::optional<IpEndPoint> local_endpoint_{};
  std::optional<IpEndPoint> remote_endpoint_{};
};

} // namespace jvs::net


#endif // !JVS_NETLIB_BSD_SOCKETS_IMPL_H_
