#include "ip_end_point.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "try_parse.h"

jvs::net::IpEndPoint::IpEndPoint(
  jvs::net::IpAddress address, jvs::net::NetworkU16 port)
  : address_(address),
  port_(port)
{
}

const jvs::net::IpAddress& jvs::net::IpEndPoint::address() const noexcept
{
  return address_;
}

jvs::net::NetworkU16 jvs::net::IpEndPoint::port() const noexcept
{
  return port_;
}

std::optional<jvs::net::IpEndPoint> jvs::net::IpEndPoint::parse(
  std::string_view ipEndPointString) noexcept
{
  std::size_t addressLen = ipEndPointString.length();
  std::size_t lastColonPos = ipEndPointString.find_last_of(':');

  // Check for an IPv6 address with a port.
  if (lastColonPos > 0 && lastColonPos != std::string::npos)
  {
    if (ipEndPointString[lastColonPos - 1] == ']')
    {
      addressLen = lastColonPos;
    }
    // Check for an IPv4 address with a port.
    else
    {
      if (ipEndPointString.substr(0, lastColonPos).find_last_of(':') ==
        std::string::npos)
      {
        addressLen = lastColonPos;
      }
    }
  }

  auto address = IpAddress::parse(ipEndPointString.substr(0, addressLen));
  if (address)
  {
    if (addressLen == ipEndPointString.length())
    {
      return IpEndPoint(*address, 0);
    }
    else
    {
      auto portValue =
        try_parse<std::uint32_t>(ipEndPointString.substr(addressLen + 1));
      if (portValue && *portValue <= 0xffff)
      {
        return IpEndPoint(*address, static_cast<std::uint16_t>(*portValue));
      }
    }
  }

  return {};
}

std::string jvs::net::to_string(const IpEndPoint& ep) noexcept
{
  return jvs::net::to_string(ep.address()) + ':' +
    std::to_string(ep.port().value());
}

std::string jvs::ConvertCast<jvs::net::IpEndPoint, std::string>::operator()(
  const jvs::net::IpEndPoint& ep) const noexcept
{
  return jvs::net::to_string(ep);
}
