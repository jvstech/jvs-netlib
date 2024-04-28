///
/// @file socket.cpp
///
/// Contains the implementations of socket wrapper functions which are
/// common between both the BSD and Winsock APIs.
///

#include <jvs-netlib/convert_cast.h>
#include <jvs-netlib/error.h>
#include <jvs-netlib/ip_address.h>
#include <jvs-netlib/ip_end_point.h>
#include <jvs-netlib/network_integers.h>
#include <jvs-netlib/socket.h>
#include <jvs-netlib/socket_context.h>
#include <jvs-netlib/socket_errors.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "native_sockets.h"
#include "socket_impl.h"
#include "socket_info.h"
#include "socket_types.h"
#include "utils.h"

#if defined(_WIN32)
#include "winsock_impl.h"
#else
#include "bsd_sockets_impl.h"
#include <jvs-netlib/socket.h>
#endif

using namespace jvs;
using namespace jvs::net;
using Family = IpAddress::Family;
using Transport = Socket::Transport;

// namespace injection for ConvertCast
namespace jvs
{

template <>
struct ConvertCast<sockaddr, jvs::net::IpEndPoint>
{
  Expected<jvs::net::IpEndPoint> operator()(const sockaddr& addr) const noexcept;
};

template <>
struct ConvertCast<sockaddr_storage, jvs::net::IpEndPoint>
{
  Expected<jvs::net::IpEndPoint> operator()(const sockaddr_storage& addr) const noexcept;
};

template <>
struct ConvertCast<IpEndPoint, sockaddr>
{
  sockaddr_storage operator()(const IpEndPoint& ep) const noexcept;
};

}  // namespace jvs

namespace
{

using GetNameFunc = decltype(::getsockname)*;

IpAddress get_ip_address(const addrinfo& addrInfo) noexcept
{
  if (addrInfo.ai_family == AF_INET)
  {
    return IpAddress(
      *alias_cast<std::uint32_t*>(&(jvs::pointer_cast<sockaddr_in>(addrInfo.ai_addr)->sin_addr)));
  }
  
  if (addrInfo.ai_family == AF_INET6)
  {
    return {&(jvs::pointer_cast<sockaddr_in6*>(addrInfo.ai_addr)->sin6_addr), Family::IPv6};
  }
  
  // Unknown address type
  return {};
}

Expected<IpEndPoint> get_endpoint(SocketContext ctx, GetNameFunc nameFunc)
{
  if (!nameFunc)
  {
    return IpEndPoint {};
  }

  sockaddr_storage addrInfo {};
  socklen_t addrLen = static_cast<socklen_t>(sizeof(addrInfo));
  int result = nameFunc(ctx, reinterpret_cast<sockaddr*>(&addrInfo), &addrLen);
  if (is_error_result(result))
  {
    return jvs::net::create_socket_error(ctx);
  }

  auto endPoint = jvs::convert_to<IpEndPoint>(addrInfo);
  if (endPoint)
  {
    return *endPoint;
  }

  return endPoint.take_error();
}

Expected<IpEndPoint> get_local_endpoint(SocketContext ctx) noexcept
{
  static_assert(std::is_same_v<GetNameFunc, decltype(::getsockname)*>);
  return get_endpoint(ctx, ::getsockname);
}

Expected<IpEndPoint> get_remote_endpoint(SocketContext ctx) noexcept
{
  static_assert(std::is_same_v<GetNameFunc, decltype(::getpeername)*>);
  return get_endpoint(ctx, ::getpeername);
}

}  // namespace

// ConvertCast specialization implementations
////////////////////////////////////////////////////////////////////////////////

Expected<IpEndPoint> ConvertCast<sockaddr, IpEndPoint>::operator()(
  const sockaddr& addr) const noexcept
{
  if (addr.sa_family == PF_INET)
  {
    auto& ipv4Addr = *reinterpret_cast<const sockaddr_in*>(&addr);
    return IpEndPoint(IpAddress(NetworkU32::from_network_order(ipv4Addr.sin_addr.s_addr).value()),
      NetworkU16::from_network_order(ipv4Addr.sin_port));
  }
  else if (addr.sa_family == PF_INET6)
  {
    auto& ipv6Addr = *reinterpret_cast<const sockaddr_in6*>(&addr);
    std::array<std::uint8_t, Ipv6AddressSize> ipv6AddrBytes {};
    std::memcpy(&ipv6AddrBytes[0], &ipv6Addr.sin6_addr, Ipv6AddressSize);
    return IpEndPoint(IpAddress(ipv6AddrBytes), NetworkU16::from_network_order(ipv6Addr.sin6_port));
  }

  // Unknown address family; emit an error.
  return jvs::net::create_socket_error(errcodes::EAFNoSupport);
}

Expected<IpEndPoint> ConvertCast<sockaddr_storage, IpEndPoint>::operator()(
  const sockaddr_storage& addr) const noexcept
{
  return convert_to<IpEndPoint>(*reinterpret_cast<const sockaddr*>(&addr));
}

sockaddr_storage ConvertCast<IpEndPoint, sockaddr>::operator()(const IpEndPoint& ep) const noexcept
{
  auto result = create_zero_filled<sockaddr_storage>();
  if (ep.address().is_ipv4())
  {
    auto addr = reinterpret_cast<sockaddr_in*>(&result);
    addr->sin_family = PF_INET;
    addr->sin_port = ep.port().network_value();
    std::memcpy(&(addr->sin_addr.s_addr), ep.address().address_bytes(), Ipv4AddressSize);
  }
  else
  {
    auto addr = reinterpret_cast<sockaddr_in6*>(&result);
    addr->sin6_family = PF_INET6;
    addr->sin6_port = ep.port().network_value();
    addr->sin6_scope_id = ep.address().scope_id();
    std::memcpy(&(addr->sin6_addr), ep.address().address_bytes(), Ipv6AddressSize);
  }

  return result;
}

// SocketInfo implementation
////////////////////////////////////////////////////////////////////////////////

SocketInfo::SocketInfo(const jvs::net::IpAddress& ipAddress)
  : address_(ipAddress), family_(address_.family())
{
}

SocketInfo::SocketInfo(const addrinfo& addrInfo)
  : address_(get_ip_address(addrInfo)), family_(get_address_family(addrInfo.ai_family))
{
  set_transports(static_cast<SocketTransport>(addrInfo.ai_socktype));
}

SocketInfo::SocketInfo(SocketContext ctx) : context_(ctx)
{
  if (auto localEndpoint = get_local_endpoint(ctx))
  {
    address_ = localEndpoint->address();
    port_ = localEndpoint->port();
    family_ = address_.family();
  }
}

const IpAddress& SocketInfo::address() const noexcept
{
  return address_;
}

Family SocketInfo::family() const noexcept
{
  return family_;
}

SocketInfo& SocketInfo::set_family(Family family) noexcept
{
  family_ = family;
  return *this;
}

Transport SocketInfo::transport() const noexcept
{
  return transport_;
}

SocketInfo& SocketInfo::set_transport(Transport transport) noexcept
{
  transport_ = transport;
  network_transport_ = get_network_transport(transport_);
  socket_transport_ = get_socket_transport(transport_);
  return *this;
}

NetworkTransport SocketInfo::network_transport() const noexcept
{
  return network_transport_;
}

SocketInfo& SocketInfo::set_network_transport(NetworkTransport transport) noexcept
{
  network_transport_ = transport;
  return *this;
}

SocketTransport SocketInfo::socket_transport() const noexcept
{
  return socket_transport_;
}

SocketInfo& SocketInfo::set_socket_transport(SocketTransport transport) noexcept
{
  socket_transport_ = transport;
  return *this;
}

// Sets all transport types to the defaults mapped from the given well-defined
// transport type.
SocketInfo& SocketInfo::set_transports(Transport transport) noexcept
{
  transport_ = transport;
  network_transport_ = get_network_transport(transport_);
  socket_transport_ = get_socket_transport(transport_);
  return *this;
}

// Sets all transport types to the defaults mapped from the given network
// transport type.
SocketInfo& SocketInfo::set_transports(NetworkTransport transport) noexcept
{
  network_transport_ = transport;
  transport_ = get_transport(network_transport_);
  socket_transport_ = get_socket_transport(network_transport_);
  return *this;
}

// Sets all transport types to the defaults mapped from the given socket
// transport type.
SocketInfo& SocketInfo::set_transports(SocketTransport transport) noexcept
{
  socket_transport_ = transport;
  transport_ = get_transport(socket_transport_);
  network_transport_ = get_network_transport(socket_transport_);
  return *this;
}

SocketContext SocketInfo::context() const noexcept
{
  return context_;
}

SocketInfo& SocketInfo::set_context(SocketContext ctx) noexcept
{
  context_ = ctx;
  return *this;
}

jvs::net::NetworkU16 SocketInfo::port() const noexcept
{
  return port_;
}

SocketInfo& SocketInfo::set_port(jvs::net::NetworkU16 port) noexcept
{
  port_ = port;
  return *this;
}

void SocketInfo::reset() noexcept
{
  address_ = IpAddress::ipv4_any();
  family_ = Family::Unspecified;
  set_transports(NetworkTransport::Unspecified);
  transport_ = Transport::Raw;
  context_ = static_cast<SocketContext>(-1);
  port_ = 0;
}

// Common Socket::SocketImpl implementations
////////////////////////////////////////////////////////////////////////////////

bool Socket::SocketImpl::update_local_endpoint() noexcept
{
  if (auto localEp = get_local_endpoint(socket_info_.context()))
  {
    local_endpoint_ = *localEp;
    return true;
  }

  return false;
}

bool Socket::SocketImpl::update_remote_endpoint() noexcept
{
  if (auto remoteEp = get_remote_endpoint(socket_info_.context()))
  {
    remote_endpoint_ = *remoteEp;
    return true;
  }

  return false;
}

// Socket implementations
////////////////////////////////////////////////////////////////////////////////

Socket::Socket(Socket&&) = default;

Socket::Socket(IpAddress::Family addressFamily, Socket::Transport transport)
  : impl_(new SocketImpl(addressFamily, transport))
{
}

Socket::Socket(Socket::SocketImpl* impl) : impl_(impl)
{
}

Socket::~Socket() = default;

IpEndPoint Socket::local() const noexcept
{
  if (impl_->local_endpoint_)
  {
    return *impl_->local_endpoint_;
  }

  return IpEndPoint(impl_->socket_info_.address(), impl_->socket_info_.port());
}

const std::optional<IpEndPoint>& Socket::remote() const noexcept
{
  return impl_->remote_endpoint_;
}

std::intptr_t Socket::descriptor() const noexcept
{
  return static_cast<std::intptr_t>(impl_->socket_info_.context());
}

Expected<Socket> Socket::accept() noexcept
{
  sockaddr_storage remoteAddrInfo {};
  socklen_t addrLen = static_cast<socklen_t>(sizeof(remoteAddrInfo));
  auto remoteCtx =
    ::accept(impl_->socket_info_.context(), reinterpret_cast<sockaddr*>(&remoteAddrInfo), &addrLen);
  if (is_error_result(remoteCtx))
  {
    return create_socket_error(impl_->socket_info_.context());
  }

  SocketImpl* impl = new SocketImpl(SocketContext(remoteCtx));
  Socket remoteSock(impl);
  remoteSock.impl_->update_remote_endpoint();
  return remoteSock;
}

Expected<IpEndPoint> Socket::bind(IpEndPoint localEndPoint) noexcept
{
  auto addrinfo = convert_to<sockaddr>(localEndPoint);
  int result = ::bind(impl_->socket_info_.context(), reinterpret_cast<sockaddr*>(&addrinfo),
    get_address_length(localEndPoint));
  if (is_error_result(result))
  {
    return create_socket_error(impl_->socket_info_.context());
  }

  impl_->update_local_endpoint();
  return local();
}

Expected<IpEndPoint> Socket::bind(IpAddress localAddress, NetworkU16 localPort) noexcept
{
  return bind(IpEndPoint(localAddress, localPort));
}

Expected<IpEndPoint> Socket::bind(IpAddress localAddress) noexcept
{
  return bind(IpEndPoint(localAddress, impl_->socket_info_.port()));
}

Expected<IpEndPoint> Socket::bind(NetworkU16 localPort) noexcept
{
  return bind(impl_->socket_info_.address(), localPort);
}

Expected<IpEndPoint> Socket::bind() noexcept
{
  return bind(impl_->socket_info_.address(), impl_->socket_info_.port());
}

Expected<IpEndPoint> Socket::connect(IpEndPoint remoteEndPoint) noexcept
{
  auto remoteInfo = convert_to<sockaddr>(remoteEndPoint);
  auto remoteLen = get_address_length(remoteEndPoint.address().family());
  auto result =
    ::connect(impl_->socket_info_.context(), reinterpret_cast<sockaddr*>(&remoteInfo), remoteLen);
  if (is_error_result(result))
  {
    return create_socket_error(impl_->socket_info_.context());
  }

  impl_->update_remote_endpoint();
  if (remote())
  {
    return *remote();
  }

  return remoteEndPoint;
}

Expected<IpEndPoint> Socket::connect(IpAddress remoteAddress, NetworkU16 remotePort) noexcept
{
  return connect(IpEndPoint(remoteAddress, remotePort));
}

Expected<IpEndPoint> Socket::listen(int backlog) noexcept
{
  int result = ::listen(impl_->socket_info_.context(), backlog);
  if (result != 0)
  {
    return create_socket_error(impl_->socket_info_.context());
  }

  impl_->update_local_endpoint();
  return local();
}

Expected<IpEndPoint> Socket::listen() noexcept
{
  return listen(SOMAXCONN);
}

Expected<std::size_t> Socket::recv(void* buffer, std::size_t length, int flags) noexcept
{
  std::size_t receivedSize = static_cast<std::size_t>(
    ::recv(impl_->socket_info_.context(), reinterpret_cast<char*>(buffer), length, flags));
  if (is_error_result(receivedSize))
  {
    return create_socket_error(impl_->socket_info_.context());
  }

  return receivedSize;
}

Expected<std::size_t> Socket::recv(void* buffer, std::size_t length) noexcept
{
  return recv(buffer, length, /*flags*/ 0);
}

Expected<std::pair<std::size_t, IpEndPoint>> Socket::recvfrom(
  void* buffer, std::size_t length, int flags) noexcept
{
  sockaddr_storage remoteInfo {};
  socklen_t remoteSize = static_cast<socklen_t>(sizeof(remoteInfo));
  std::size_t receivedSize = static_cast<std::size_t>(
    ::recvfrom(impl_->socket_info_.context(), reinterpret_cast<char*>(buffer), length, flags,
      reinterpret_cast<sockaddr*>(&remoteInfo), &remoteSize));
  if (is_error_result(receivedSize))
  {
    return create_socket_error(impl_->socket_info_.context());
  }

  if (remoteSize != 0)
  {
    auto remoteEndpoint = convert_to<IpEndPoint>(remoteInfo);
    if (remoteEndpoint)
    {
      impl_->remote_endpoint_ = *remoteEndpoint;
      return std::make_pair(receivedSize, *remoteEndpoint);
    }

    return remoteEndpoint.take_error();
  }

  return std::make_pair(receivedSize, IpEndPoint {});
}

Expected<std::pair<std::size_t, IpEndPoint>> Socket::recvfrom(
  void* buffer, std::size_t length) noexcept
{
  return recvfrom(buffer, length, /*flags*/ 0);
}

Expected<std::size_t> Socket::send(const void* buffer, std::size_t length, int flags) noexcept
{
  std::size_t sentSize = static_cast<std::size_t>(
    ::send(impl_->socket_info_.context(), static_cast<const char*>(buffer), length, flags));
  if (is_error_result(sentSize))
  {
    return create_socket_error(impl_->socket_info_.context());
  }

  return sentSize;
}

Expected<std::size_t> Socket::send(const void* buffer, std::size_t length) noexcept
{
  return send(buffer, length, /*flags*/ 0);
}

Expected<std::size_t> Socket::sendto(
  const void* buffer, std::size_t length, int flags, const IpEndPoint& remoteEp) noexcept
{
  auto remoteInfo = convert_to<sockaddr>(remoteEp);
  auto addrLen = get_address_length(remoteEp);
  std::size_t sentSize = static_cast<std::size_t>(
    ::sendto(impl_->socket_info_.context(), static_cast<const char*>(buffer), length, flags,
      pointer_cast<const sockaddr*>(&remoteInfo), addrLen));
  if (is_error_result(sentSize))
  {
    return create_socket_error(impl_->socket_info_.context());
  }

  return sentSize;
}

Expected<std::size_t> Socket::sendto(
  const void* buffer, std::size_t length, const IpEndPoint& remoteEp) noexcept
{
  return sendto(buffer, length, /*flags*/ 0, remoteEp);
}

jvs::Expected<std::optional<std::string>> jvs::net::read(Socket& s)
{
  char startBuf = 0;
  auto recvResult = s.recv(&startBuf, 0);
  if (auto e = recvResult.take_error())
  {
    return std::move(e);
  }

  auto bytesToRead = s.available();
  if (auto e = bytesToRead.take_error())
  {
    return std::move(e);
  }

  std::optional<std::string> buffer(std::in_place, *bytesToRead, static_cast<char>(0));
  auto bytesRead = s.recv(buffer->data(), buffer->size());
  if (auto e = bytesRead.take_error())
  {
    return std::move(e);    
  }

  if (!*bytesRead)
  {
    // Connection was closed.
    return std::nullopt;
  }

  return buffer;
}

jvs::Error jvs::net::write(Socket& s, std::string_view data)
{
  auto bytesSent = s.send(data.data(), data.length());
  if (auto e = bytesSent.take_error())
  {
    return e;
  }

  std::size_t totalSent = *bytesSent;
  while (totalSent < data.length())
  {
    bytesSent = s.send(data.data() + totalSent, data.length() - totalSent);
    if (auto e = bytesSent.take_error())
    {
      return e;
    }

    totalSent += *bytesSent;
  }

  return Error::success();
}
