#if !defined(JVS_NETLIB_IP_END_POINT_H_)
#define JVS_NETLIB_IP_END_POINT_H_

#include <optional>
#include <string>
#include <string_view>

#include "convert_cast.h"
#include "ip_address.h"
#include "network_integers.h"

namespace jvs::net
{

struct IpEndPoint final
{

  IpEndPoint() = default;

  IpEndPoint(IpAddress address, NetworkU16 port);

  const IpAddress& address() const noexcept;

  NetworkU16 port() const noexcept;

  //
  // named constructors
  //

  static std::optional<IpEndPoint> parse(std::string_view ipEndPointString) 
    noexcept;

private:
  IpAddress address_{};
  NetworkU16 port_{0};
};

std::string to_string(const IpEndPoint& ep) noexcept;

} // namespace jvs::net

namespace jvs
{

template <>
struct ConvertCast<net::IpEndPoint, std::string>
{
  std::string operator()(const net::IpEndPoint& ep) const noexcept;
};

} // namespace jvs



#endif // !JVS_NETLIB_IP_END_POINT_H_
