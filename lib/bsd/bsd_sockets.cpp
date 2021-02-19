#include "socket.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <memory>
#include <type_traits>
#include <tuple>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "bsd_socket_impl.h"
#include "bsd_socket_info.h"
#include "bsd_sockets.h"
#include "convert_cast.h"
#include "network_integers.h"
#include "utils.h"

namespace
{

using bsd::SocketInfo;
using jvs::net::IpAddress;
using jvs::net::IpEndPoint;
using jvs::net::Socket;
using Transport = Socket::Transport;
using Family = IpAddress::Family;

inline constexpr std::size_t ErrorSize = static_cast<std::size_t>(-1);

} // namespace `anonymous-namespace'

addrinfo bsd::create_empty_hints() noexcept
{
  return jvs::create_zero_filled<addrinfo>();
}

addrinfo bsd::create_hints(
  IpAddress::Family addressFamily, Socket::Transport transport) noexcept
{
  auto hints = create_empty_hints();
  hints.ai_family = get_address_family(addressFamily);
  hints.ai_socktype = static_cast<int>(get_socket_transport(transport));
  hints.ai_flags = AI_PASSIVE; // let the system choose an address
  return hints;
}

IpAddress bsd::get_ip_address(const addrinfo& addrInfo) noexcept
{
  if (addrInfo.ai_family == AF_INET)
  {
    return IpAddress(*jvs::net::alias_cast<std::uint32_t*>(
      &(reinterpret_cast<sockaddr_in*>(addrInfo.ai_addr)->sin_addr)));
  }
  else if (addrInfo.ai_family == AF_INET6)
  {
    return IpAddress(
      &(reinterpret_cast<sockaddr_in6*>(addrInfo.ai_addr)->sin6_addr),
      IpAddress::Family::IPv6);
  }
  else
  {
    // Unknown address type
    return IpAddress{};
  }
}

jvs::Error bsd::create_addrinfo_error(int ecode) noexcept
{
  return jvs::make_error<jvs::net::SocketError>(ecode, gai_strerror(ecode));
}

jvs::Error bsd::create_socket_error(int ecode) noexcept
{
  if (ecode == EAGAIN || ecode == EWOULDBLOCK)
  {
    return jvs::make_error<jvs::net::NonBlockingStatus>();
  }

  return jvs::make_error<jvs::net::SocketError>(ecode, strerror(ecode));
}


sockaddr_storage jvs::ConvertCast<jvs::net::IpEndPoint, sockaddr>::operator()(
  const net::IpEndPoint& ep) const noexcept
{
  auto result = create_zero_filled<sockaddr_storage>();
  if (ep.address().is_ipv4())
  {
    auto addr = reinterpret_cast<sockaddr_in*>(&result);
    addr->sin_family = PF_INET;
    addr->sin_port = ep.port().network_value();
    std::memcpy(&(addr->sin_addr.s_addr), ep.address().address_bytes(),
      net::Ipv4AddressSize);
  }
  else
  {
    auto addr = reinterpret_cast<sockaddr_in6*>(&result);
    addr->sin6_family = PF_INET6;
    addr->sin6_port = ep.port().network_value();
    addr->sin6_scope_id = ep.address().scope_id();
    std::memcpy(
      &(addr->sin6_addr), ep.address().address_bytes(), net::Ipv6AddressSize);
  }

  return result;
}

auto jvs::ConvertCast<sockaddr, jvs::net::IpEndPoint>::operator()(
  const sockaddr& addr) const noexcept
  -> jvs::Expected<jvs::net::IpEndPoint>
{
  if (addr.sa_family == PF_INET)
  {
    auto& ipv4addr = *reinterpret_cast<const sockaddr_in*>(&addr);
    return net::IpEndPoint(
      net::IpAddress(
        net::NetworkU32::from_network_order(
          ipv4addr.sin_addr.s_addr).value()),
      net::NetworkU16::from_network_order(ipv4addr.sin_port));
  }
  else if (addr.sa_family == PF_INET6)
  {
    auto& ipv6addr = *reinterpret_cast<const sockaddr_in6*>(&addr);
    std::array<std::uint8_t, net::Ipv6AddressSize> ipv6AddrBytes{};
    std::memcpy(&ipv6AddrBytes[0], &ipv6addr.sin6_addr, net::Ipv6AddressSize);
    return net::IpEndPoint(net::IpAddress(ipv6AddrBytes), 
      net::NetworkU16::from_network_order(ipv6addr.sin6_port));
  }

  return jvs::make_error<net::UnsupportedOperationError>();
}

jvs::Expected<jvs::net::IpEndPoint> bsd::get_remote_endpoint(
  bsd::SocketHandle sockfd) noexcept
{
  sockaddr_storage remoteAddrInfo{};
  socklen_t addrLen{};
  int result = ::getpeername(sockfd,
    reinterpret_cast<sockaddr*>(&remoteAddrInfo), &addrLen);
  if (result == -1)
  {
    return create_socket_error(errno);
  }

  auto remoteEndPoint = jvs::convert_to<jvs::net::IpEndPoint>(remoteAddrInfo);
  if (remoteEndPoint)
  {
    return *remoteEndPoint;
  }

  return remoteEndPoint.take_error();
}

jvs::Expected<bsd::SocketInfo> bsd::create_socket(
  IpAddress::Family addressFamily, Socket::Transport transport) noexcept
{
  auto hints = bsd::create_hints(addressFamily, transport);
  addrinfo* ainfo;

  int status = getaddrinfo(nullptr, "0", &hints, &ainfo);
  if (status == 0)
  {
    SocketInfo result(*ainfo);
    freeaddrinfo(ainfo);
    int sockfd = socket(bsd::get_address_family(result.family()),
      static_cast<int>(result.socket_transport()),
      static_cast<int>(result.network_transport()));
    result.set_descriptor(sockfd);
    if (sockfd != -1)
    {
      return result;
    }
    else
    {
      return bsd::create_socket_error(errno);
    }
  }
  else
  {
    return bsd::create_addrinfo_error(status);
  }
}

jvs::Expected<bsd::SocketInfo> bsd::create_socket(
  Socket::Transport transport) noexcept
{
  return bsd::create_socket(Family::Unspecified, transport);
}

auto jvs::ConvertCast<sockaddr_storage, jvs::net::IpEndPoint>::operator()(
  const sockaddr_storage& addr) const noexcept
  -> jvs::Expected<net::IpEndPoint>
{
  return convert_to<net::IpEndPoint>(
    *reinterpret_cast<const sockaddr*>(&addr));
}

jvs::Expected<jvs::net::IpEndPoint> bsd::get_local_endpoint(
  SocketHandle sockfd) noexcept
{
  sockaddr_storage localAddrInfo{};
  socklen_t addrLen{};
  int result = ::getsockname(sockfd, 
    reinterpret_cast<sockaddr*>(&localAddrInfo), &addrLen);
  if (result == -1)
  {
    return create_socket_error(errno);
  }

  auto localEndPoint = jvs::convert_to<jvs::net::IpEndPoint>(localAddrInfo);
  if (localEndPoint)
  {
    return *localEndPoint;
  }

  return localEndPoint.take_error();
}

jvs::net::Socket::SocketImpl::SocketImpl(
  Family addressFamily, Socket::Transport transport)
{
  if (auto si = bsd::create_socket(addressFamily, transport))
  {
    socket_info_ = *si;
  }
  else
  {
    jvs::consume_error(si.take_error());
  }
}

jvs::net::Socket::SocketImpl::SocketImpl(Socket::Transport transport)
  : SocketImpl(Family::Unspecified, transport)
{
}

jvs::net::Socket::SocketImpl::SocketImpl(int sockfd)
  : socket_info_(sockfd)
{
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::SocketImpl::bind(
  IpEndPoint localEndPoint) noexcept
{
  auto addrInfo = convert_to<sockaddr>(localEndPoint);
  int result =
    ::bind(socket_info_.descriptor(), reinterpret_cast<sockaddr*>(&addrInfo),
      bsd::get_address_length(localEndPoint));
  if (result != 0)
  {      
    return bsd::create_socket_error(errno);
  }

  update_local_endpoint();
  return local();
}

bool jvs::net::Socket::SocketImpl::update_local_endpoint() noexcept
{
  if (auto localEndpoint = bsd::get_local_endpoint(socket_info_.descriptor()))
  {
    local_endpoint_ = *localEndpoint;
    return true;
  }

  return false;
}

bool jvs::net::Socket::SocketImpl::update_remote_endpoint() noexcept
{
  if (auto remoteEndpoint = bsd::get_remote_endpoint(socket_info_.descriptor()))
  {
    remote_endpoint_ = *remoteEndpoint;
    return true;
  }

  return false;
}

//!
//! Accepts a connection
//!
//! @returns a new Socket for communicating with the remote endpoint of the
//!          accepted connection or a socket error if the connection failed.
//!
jvs::Expected<Socket> jvs::net::Socket::SocketImpl::accept() noexcept
{
  sockaddr_storage remoteAddrInfo{};
  socklen_t addrLen;
  bsd::SocketHandle remoteFd = ::accept(socket_info_.descriptor(),
    reinterpret_cast<sockaddr*>(&remoteAddrInfo), &addrLen);
  if (remoteFd == -1)
  {
    return bsd::create_socket_error(errno);
  }

  SocketImpl* impl = new SocketImpl(remoteFd);
  Socket remoteSock(impl);
  return remoteSock;
}

jvs::Expected<IpEndPoint> jvs::net::Socket::SocketImpl::bind(
  IpAddress address, NetworkU16 port) noexcept
{
  return bind(IpEndPoint(address, port));
}

jvs::Expected<IpEndPoint> jvs::net::Socket::SocketImpl::bind(
  IpAddress localAddress) noexcept
{
  return bind(IpEndPoint(localAddress, socket_info_.port()));
}

jvs::Expected<IpEndPoint> jvs::net::Socket::SocketImpl::bind(
  NetworkU16 port) noexcept
{
  return bind(IpEndPoint(socket_info_.address(), port));
}

jvs::Expected<IpEndPoint> jvs::net::Socket::SocketImpl::bind() noexcept
{
  return bind(IpEndPoint(socket_info_.address(), socket_info_.port()));
}

int jvs::net::Socket::SocketImpl::close() noexcept
{
  int closeResult = ::close(socket_info_.descriptor());
  socket_info_.reset();
  local_endpoint_.reset();
  remote_endpoint_.reset();
  return closeResult;
}

jvs::Expected<IpEndPoint> jvs::net::Socket::SocketImpl::connect(
  IpEndPoint remoteEndPoint) noexcept
{
  auto remoteInfo = jvs::convert_to<sockaddr>(remoteEndPoint);
  auto remoteLen = bsd::get_address_length(remoteEndPoint.address().family());
  auto result = ::connect(socket_info_.descriptor(),
    reinterpret_cast<sockaddr*>(&remoteInfo), remoteLen);
  if (result == -1)
  {
    return bsd::create_socket_error(errno);
  }

  update_remote_endpoint();
  if (remote())
  {
    return *remote();
  }
    
  return remoteEndPoint;
}

jvs::Expected<IpEndPoint> jvs::net::Socket::SocketImpl::listen(
  int backlog) noexcept
{
  int result = ::listen(socket_info_.descriptor(), backlog);
  if (result != 0)
  {
    return bsd::create_socket_error(errno);
  }

  update_local_endpoint();
  return local();
}

jvs::Expected<std::size_t> jvs::net::Socket::SocketImpl::recv(
  void* buffer, std::size_t length, int flags) noexcept
{
  std::size_t receivedSize = static_cast<std::size_t>(
    ::recv(socket_info_.descriptor(), buffer, length, flags));
  if (receivedSize == ErrorSize)
  {
    return bsd::create_socket_error(errno);
  }

  return receivedSize;
}

auto jvs::net::Socket::SocketImpl::recvfrom(
  void* buffer, std::size_t length, int flags) noexcept
  -> jvs::Expected<std::pair<std::size_t, IpEndPoint>>
{
  sockaddr_storage remoteInfo{};
  socklen_t remoteSize{};
  std::size_t receivedSize =
    static_cast<std::size_t>(::recvfrom(socket_info_.descriptor(), buffer,
      length, flags, reinterpret_cast<sockaddr*>(&remoteInfo), &remoteSize));
  if (receivedSize == ErrorSize)
  {
    return bsd::create_socket_error(errno);
  }

  auto remoteEndpoint = convert_to<IpEndPoint>(remoteInfo);
  if (remoteEndpoint)
  {
    return std::make_pair(receivedSize, *remoteEndpoint);
  }

  return remoteEndpoint.take_error();
}

jvs::Expected<std::size_t> jvs::net::Socket::SocketImpl::send(
  const void* buffer, std::size_t length, int flags) noexcept
{
  std::size_t sentSize = static_cast<std::size_t>(
    ::send(socket_info_.descriptor(), buffer, length, flags));
  if (sentSize == ErrorSize)
  {
    return bsd::create_socket_error(errno);
  }

  return sentSize;
}

jvs::Expected<std::size_t> jvs::net::Socket::SocketImpl::sendto(
  const void* buffer, std::size_t length, int flags, const IpEndPoint& remoteEp) 
  noexcept
{
  auto remoteInfo = jvs::convert_to<sockaddr>(remoteEp);
  auto addrLen = bsd::get_address_length(remoteEp);
  std::size_t sentSize = static_cast<std::size_t>(
    ::sendto(socket_info_.descriptor(), buffer, length, flags,
      reinterpret_cast<const sockaddr*>(&remoteInfo), addrLen));
  if (sentSize == ErrorSize)
  {
    return bsd::create_socket_error(errno);
  }

  return sentSize;
}


jvs::net::Socket::Socket(Socket&&) = default;

jvs::net::Socket::~Socket() = default;

jvs::net::Socket::Socket(Transport transport)
  : impl_(new SocketImpl(transport))
{
}

jvs::net::Socket::Socket(IpAddress::Family addressFamily, Transport transport)
  : impl_(new SocketImpl(addressFamily, transport))
{
}

jvs::net::Socket::Socket(Socket::SocketImpl* impl)
  : impl_(impl)
{
}

jvs::net::IpEndPoint jvs::net::Socket::local() const noexcept
{
  return impl_->local();
}

const std::optional<jvs::net::IpEndPoint>& jvs::net::Socket::remote() const
  noexcept
{
  return impl_->remote();
}

//!
//! Accepts a connection
//!
//! @returns a new Socket for communicating with the remote endpoint of the
//!          accepted connection or a socket error if the connection failed.
//!
jvs::Expected<jvs::net::Socket> jvs::net::Socket::accept() noexcept
{
  return impl_->accept();
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::bind(
  IpEndPoint localEndPoint) noexcept
{
  return impl_->bind(localEndPoint);
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::bind(
  IpAddress localAddress, NetworkU16 localPort) noexcept
{
  return impl_->bind(localAddress, localPort);
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::bind(
  IpAddress localAddress) noexcept
{
  return impl_->bind(localAddress, 0);
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::bind(
  NetworkU16 localPort) noexcept
{
  return impl_->bind(localPort);
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::bind() noexcept
{
  return impl_->bind();
}

int jvs::net::Socket::close() noexcept
{
  return impl_->close();
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::connect(
  IpEndPoint remoteEndpoint) noexcept
{
  return impl_->connect(remoteEndpoint);
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::connect(
  IpAddress remoteAddress, jvs::net::NetworkU16 remotePort) noexcept
{
  return impl_->connect(IpEndPoint(remoteAddress, remotePort));
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::listen() noexcept
{
  return impl_->listen(SOMAXCONN);
}

jvs::Expected<jvs::net::IpEndPoint> jvs::net::Socket::listen(
  int backlog) noexcept
{
  return impl_->listen(backlog);
}

jvs::Expected<std::size_t> jvs::net::Socket::recv(
  void* buffer, std::size_t length) noexcept
{
  return impl_->recv(buffer, length, 0);
}

jvs::Expected<std::size_t> jvs::net::Socket::recv(
  void* buffer, std::size_t length, int flags) noexcept
{
  return impl_->recv(buffer, length, flags);
}

auto jvs::net::Socket::recvfrom(void* buffer, std::size_t length, int flags) 
  noexcept
  -> jvs::Expected<std::pair<std::size_t, jvs::net::IpEndPoint>>
{
  return impl_->recvfrom(buffer, length, flags);
}

auto jvs::net::Socket::recvfrom(void* buffer, std::size_t length) noexcept
  -> jvs::Expected<std::pair<std::size_t, jvs::net::IpEndPoint>>
{
  return impl_->recvfrom(buffer, length, 0);
}

jvs::Expected<std::size_t> jvs::net::Socket::send(
  const void* buffer, std::size_t length, int flags) noexcept
{
  return impl_->send(buffer, length, flags);
}

jvs::Expected<std::size_t> jvs::net::Socket::send(
  const void* buffer, std::size_t length) noexcept
{
  return impl_->send(buffer, length, 0);
}

jvs::Expected<std::size_t> jvs::net::Socket::sendto(const void* buffer, 
  std::size_t length, int flags, const IpEndPoint& remoteEp) noexcept
{
  return impl_->sendto(buffer, length, flags, remoteEp);
}

jvs::Expected<std::size_t> jvs::net::Socket::sendto(const void* buffer, 
  std::size_t length, const IpEndPoint& remoteEp) noexcept
{
  return impl_->sendto(buffer, length, 0, remoteEp);
}
