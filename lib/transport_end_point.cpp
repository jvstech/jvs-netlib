#include "transport_end_point.h"

#include <algorithm>
#include <cctype>

jvs::net::TransportEndPoint::TransportEndPoint(
  jvs::net::IpEndPoint ipEndPoint, jvs::net::Socket::Transport transport)
  : ip_end_point_(ipEndPoint),
  transport_(transport)
{
}

const jvs::net::IpEndPoint& jvs::net::TransportEndPoint::ip_end_point()
  const noexcept
{
  return ip_end_point_;
}

jvs::net::Socket::Transport jvs::net::TransportEndPoint::transport()
  const noexcept
{
  return transport_;
}

std::optional<jvs::net::TransportEndPoint> jvs::net::TransportEndPoint::parse(
  std::string_view transportEndPointString) noexcept
{
  std::size_t epLen = transportEndPointString.length();
  std::size_t slashPos = transportEndPointString.find('/');

  if (slashPos > 0 && slashPos != std::string_view::npos)
  {
    epLen = slashPos;
  }

  auto ipEndPoint = IpEndPoint::parse(transportEndPointString.substr(0, epLen));
  if (ipEndPoint)
  {
    if (epLen == transportEndPointString.length())
    {
      return TransportEndPoint(*ipEndPoint, Socket::Transport::Tcp);
    }
    else
    {
      auto lcase = [](int c) { return std::tolower(c); };
      std::string transportString(transportEndPointString.substr(epLen + 1));
      std::transform(transportString.begin(), transportString.end(),
        transportString.begin(), lcase);
      if (transportString == "tcp")
      {
        return TransportEndPoint(*ipEndPoint, Socket::Transport::Tcp);
      }
      else if (transportString == "udp")
      {
        return TransportEndPoint(*ipEndPoint, Socket::Transport::Udp);
      }
      else if (transportString == "raw")
      {
        return TransportEndPoint(*ipEndPoint, Socket::Transport::Raw);
      }
    }
  }

  return {};
}

std::string jvs::net::to_string(const TransportEndPoint& ep) noexcept
{
  std::string result = to_string(ep.ip_end_point());
  result.reserve(4);
  switch (ep.transport())
  {
  case Socket::Transport::Tcp:
    return result.append("/tcp");
  case Socket::Transport::Udp:
    return result.append("/udp");
  case Socket::Transport::Raw:
    return result.append("/raw");
  }
}

std::string jvs::ConvertCast<jvs::net::TransportEndPoint,
  std::string>::operator()(const jvs::net::TransportEndPoint& ep) const noexcept
{
  return jvs::net::to_string(ep);
}
