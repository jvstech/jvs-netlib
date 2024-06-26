///
/// @file socket_impl.h
/// 
/// Contains private declarations of functions used in multiple translation
/// units for both the BSD and Winsock APIs.
/// 

#if !defined(JVS_NETLIB_SOCKET_IMPL_H_)
#define JVS_NETLIB_SOCKET_IMPL_H_

#include <string>

#include <jvs-netlib/error.h>
#include <jvs-netlib/ip_address.h>
#include <jvs-netlib/ip_end_point.h>
#include <jvs-netlib/socket.h>

#include "socket_info.h"

namespace jvs::net
{

addrinfo create_empty_hints() noexcept;

addrinfo create_hints(
  IpAddress::Family addressFamily, Socket::Transport transport) noexcept;

jvs::Expected<SocketInfo> create_socket(
  IpAddress::Family addressFamily, Socket::Transport transport) noexcept;

jvs::Error create_socket_error(SocketContext s) noexcept;

template <typename ResultT>
static constexpr bool is_error_result(ResultT result) noexcept
{
  return (result == static_cast<ResultT>(-1));
}

jvs::Error create_socket_error(int ecode) noexcept;

jvs::Error create_addrinfo_error(int ecode) noexcept;

// Declarations that must be implemented by each socket-API wrapping
// implementation
////////////////////////////////////////////////////////////////////////////////


int get_last_error() noexcept;

std::string get_socket_error_message(int ecode) noexcept;

std::string get_addrinfo_error_message(int ecode) noexcept;


} // namespace jvs::net


#endif // !JVS_NETLIB_SOCKET_IMPL_H_
