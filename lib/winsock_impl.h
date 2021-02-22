//!
//! @file winsock_impl.h
//! 
//! Contains the declarations required for the Winsock-specific implementation 
//! of Socket::SocketImpl.
//! 

#if !defined(JVS_NETLIB_WINSOCK_IMPL_H_)
#define JVS_NETLIB_WINSOCK_IMPL_H_

#include <optional>

#include "native_socket_includes.h"

#include "ip_end_point.h"
#include "socket.h"

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

  int startup_code_{WSAVERNOTSUPPORTED};
  WSADATA data_{};
  SocketInfo socket_info_{};
  std::optional<IpEndPoint> local_endpoint_{};
  std::optional<IpEndPoint> remote_endpoint_{};
};

} // namespace jvs::net


#endif // !JVS_NETLIB_WINSOCK_IMPL_H_
