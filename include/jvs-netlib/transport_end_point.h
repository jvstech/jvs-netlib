///
/// @file transport_endpoint.h
/// 
/// Contains the declarations for jvs::net::TransportEndPoint.

#if !defined(JVS_NETLIB_TRANSPORT_ENDPOINT_H_)
#define JVS_NETLIB_TRANSPORT_ENDPOINT_H_

#include <optional>
#include <string>
#include <string_view>

#include "convert_cast.h"
#include "ip_address.h"
#include "ip_end_point.h"
#include "network_integers.h"
#include "socket.h"

namespace jvs::net
{

///
/// @class TransportEndPoint
/// 
/// Named tuple representing one end of a bound socket containing an IP end 
/// point and a transport protocol.
/// 
class TransportEndPoint final
{
public:
  TransportEndPoint() = default;

  TransportEndPoint(IpEndPoint ipEndPoint, Socket::Transport transport);

  const IpEndPoint& ip_end_point() const noexcept;

  Socket::Transport transport() const noexcept;

  const IpAddress& address() const noexcept
  {
    return ip_end_point_.address();
  }

  NetworkU16 port() const noexcept
  {
    return ip_end_point_.port();
  }

  static std::optional<TransportEndPoint> parse(
    std::string_view transportEndPointString) noexcept;

private:
  IpEndPoint ip_end_point_{};
  Socket::Transport transport_{Socket::Transport::Tcp};
};

std::string to_string(const TransportEndPoint& ep) noexcept;

} // namespace jvs::net

namespace jvs
{

template <>
struct ConvertCast<net::TransportEndPoint, std::string>
{
  std::string operator()(const net::TransportEndPoint& ep) const noexcept;
};

} // namespace jvs



#endif // !JVS_NETLIB_TRANSPORT_ENDPOINT_H_
