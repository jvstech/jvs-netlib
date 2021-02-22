#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "gtest/gtest.h"

#include "ip_address.h"
#include "network_integers.h"
#include "socket.h"

namespace
{

#if defined(_WIN32)


// Winsock versions
static constexpr WORD WinsockVersions[] = {
  MAKEWORD(2, 2),
  MAKEWORD(2, 1),
  MAKEWORD(2, 0),
  MAKEWORD(1, 1),
  MAKEWORD(1, 0)
};

static void initSockets() noexcept
{
  WSADATA data{};
  int statusCode{WSAVERNOTSUPPORTED};
  // Try to initialize the Winsock version in descending order.
  for (int i = 0; i < (sizeof(WinsockVersions) / sizeof(WORD)) &&
    statusCode == WSAVERNOTSUPPORTED; ++i)
  {
    statusCode = ::WSAStartup(WinsockVersions[i], &data);
  }
}

static void termSockets() noexcept
{
  ::WSACleanup();
}

#else

static void initSockets() noexcept
{
  // Do nothing.
}

static void termSockets() noexcept
{
  // Do nothing.
}

#endif // _WIN32

} // namespace 


namespace jvs
{

template <>
struct ConvertCast<addrinfo, net::IpAddress>
{
  net::IpAddress operator()(const addrinfo& addrInfo) const
  {
    if (addrInfo.ai_family == AF_INET)
    {
      return net::IpAddress(*jvs::net::alias_cast<std::uint32_t*>(
        &(reinterpret_cast<sockaddr_in*>(addrInfo.ai_addr)->sin_addr)));
    }
    else if (addrInfo.ai_family == AF_INET6)
    {
      return net::IpAddress(
        &(reinterpret_cast<sockaddr_in6*>(addrInfo.ai_addr)->sin6_addr),
        net::IpAddress::Family::IPv6);
    }
    else
    {
      // Unknown address type
      return net::IpAddress::ipv6_any();
    }
  }
};

template <>
struct ConvertCast<net::IpAddress::Family, std::string>
{
  std::string_view operator()(const net::IpAddress::Family family)
  {
    if (family == net::IpAddress::Family::IPv4)
    {
      return "IPv4";
    }
    else
    {
      return "IPv6";
    }
  }
};

} // namespace jvs


TEST(SocketTest, GetLocalIPv4Address)
{
  addrinfo hints;
  addrinfo* ainfo;

  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;    // let the system choose an address

  initSockets();

  int status = getaddrinfo(nullptr, "0", &hints, &ainfo);
  EXPECT_EQ(status, 0);
  if (status == 0)
  {
    auto addr = jvs::convert_to<jvs::net::IpAddress>(*ainfo);
    auto ipver = jvs::convert_to<std::string>(addr.family());
    EXPECT_NE(addr, jvs::net::IpAddress::ipv6_any());
    std::cout << ipver << ": " << jvs::convert_to<std::string>(addr) << '\n';
    freeaddrinfo(ainfo);
  }
  else
  {
    std::cerr << "getaddrinfo() result = " << status << '\n'
      << gai_strerror(status) << '\n';
  }

  termSockets();
}

TEST(SocketTest, BindSpecificIpv4)
{
  jvs::net::Socket tcpSock(jvs::net::IpAddress::Family::IPv4,
    jvs::net::Socket::Transport::Tcp);
  auto boundTcpEndpoint =
    tcpSock.bind(*jvs::net::IpEndPoint::parse("127.0.0.1:1234"));
  EXPECT_TRUE(static_cast<bool>(boundTcpEndpoint));
  tcpSock.close();

  jvs::net::Socket udpSock(jvs::net::IpAddress::Family::IPv4,
    jvs::net::Socket::Transport::Udp);
  auto boundUdpEndpoint =
    udpSock.bind(*jvs::net::IpEndPoint::parse("127.0.0.1:5678"));
  EXPECT_TRUE(static_cast<bool>(boundUdpEndpoint));
  udpSock.close();
}

TEST(SocketTest, BindAnyIpv4)
{
  jvs::net::Socket tcpSock(jvs::net::IpAddress::Family::IPv4, 
    jvs::net::Socket::Transport::Tcp);
  auto boundTcpEndpoint = tcpSock.bind(1234);
  EXPECT_TRUE(static_cast<bool>(boundTcpEndpoint));
  tcpSock.close();

  jvs::net::Socket udpSock(jvs::net::IpAddress::Family::IPv6,
    jvs::net::Socket::Transport::Udp);
  auto boundUdpEndpoint = udpSock.bind(5678);
  EXPECT_TRUE(static_cast<bool>(boundUdpEndpoint));
  udpSock.close();
}

TEST(SocketTest, BindSpecificIpv6)
{
  jvs::net::Socket tcpSock(jvs::net::IpAddress::Family::IPv6,
    jvs::net::Socket::Transport::Tcp);
  auto boundTcpEndpoint =
    tcpSock.bind(*jvs::net::IpEndPoint::parse("[::1]:1234"));
  EXPECT_TRUE(static_cast<bool>(boundTcpEndpoint));
  tcpSock.close();

  jvs::net::Socket udpSock(jvs::net::IpAddress::Family::IPv6,
    jvs::net::Socket::Transport::Udp);
  auto boundUdpEndpoint =
    udpSock.bind(*jvs::net::IpEndPoint::parse("[::1]:5678"));
  EXPECT_TRUE(static_cast<bool>(boundUdpEndpoint));
  udpSock.close();
}

TEST(SocketTest, BindAnyIpv6)
{
  jvs::net::Socket tcpSock(jvs::net::IpAddress::Family::IPv6, 
    jvs::net::Socket::Transport::Tcp);
  auto boundTcpEndpoint = tcpSock.bind(1234);
  EXPECT_TRUE(static_cast<bool>(boundTcpEndpoint));
  tcpSock.close();

  jvs::net::Socket udpSock(jvs::net::IpAddress::Family::IPv6,
    jvs::net::Socket::Transport::Udp);
  auto boundUdpEndpoint = udpSock.bind(5678);
  EXPECT_TRUE(static_cast<bool>(boundUdpEndpoint));
  udpSock.close();
}

TEST(SocketTest, ListenTcpv4)
{
  jvs::net::Socket sock(jvs::net::IpAddress::Family::IPv4,
    jvs::net::Socket::Transport::Tcp);
  if (auto boundEp = sock.bind(1234))
  {
    auto listenEndpoint = sock.listen();
    EXPECT_TRUE(static_cast<bool>(listenEndpoint));
    if (!listenEndpoint)
    {
      jvs::handle_all_errors(listenEndpoint.take_error(),
        [&](const jvs::net::SocketError& e)
        {
          e.log(std::cerr);
        });
    }
  }
  else
  {
    EXPECT_FALSE("Failed to bind TCP socket");
  }
  
  sock.close();
}

TEST(SocketTest, ListenTcpv6)
{
  jvs::net::Socket sock(jvs::net::IpAddress::Family::IPv6,
    jvs::net::Socket::Transport::Tcp);
  if (auto boundEp = sock.bind(1234))
  {
    auto listenEndpoint = sock.listen();
    EXPECT_TRUE(static_cast<bool>(listenEndpoint));
    if (!listenEndpoint)
    {
      jvs::handle_all_errors(listenEndpoint.take_error(),
        [&](const jvs::net::SocketError& e)
        {
          e.log(std::cerr);
        });
    }
  }
  else
  {
    EXPECT_FALSE("Failed to bind TCPv6 socket");
  }

  sock.close();
}

TEST(SocketTest, HelloBlockingTcpv4)
{
  auto handleSocketError = [&](const jvs::net::SocketError& e)
  {
    e.log(std::cerr);
  };

  auto serverTask = std::async(std::launch::async, [&handleSocketError]()
  {
    jvs::net::Socket server(jvs::net::IpAddress::Family::IPv4,
      jvs::net::Socket::Transport::Tcp);
    bool serverResult{false};
    auto boundEp = server.bind(1234);
    if (boundEp)
    {
      auto listenEp = server.listen();
      if (listenEp)
      {
        auto connection = server.accept();
        if (connection)
        {
          auto sentBytes = connection->send("Hello, world!\n", 14);
          if (sentBytes)
          {
            std::array<std::uint8_t, 64> receivedBytes{};
            auto receivedSize =
              connection->recv(&receivedBytes[0], receivedBytes.size());
            if (receivedSize)
            {
              std::string_view cs(
                jvs::net::alias_cast<const char*>(receivedBytes.data()),
                *receivedSize);
              serverResult = (cs == "Hello, server!\n");
            }
            else
            {
              jvs::handle_all_errors(receivedSize.take_error(), 
                handleSocketError);
            }
          }
          else
          {
            jvs::handle_all_errors(sentBytes.take_error(), handleSocketError);
          }

          connection->close();
        }
        else
        {
          jvs::handle_all_errors(connection.take_error(), handleSocketError);
        }
      }
      else
      {
        jvs::handle_all_errors(listenEp.take_error(), handleSocketError);
      }
    }
    else
    {
      jvs::handle_all_errors(boundEp.take_error(), handleSocketError);
    }

    server.close();
    return serverResult;
  });
  

  auto clientTask = std::async(std::launch::async, [&handleSocketError]()
  {
    jvs::net::Socket client(jvs::net::IpAddress::Family::IPv4,
      jvs::net::Socket::Transport::Tcp);
    bool clientResult{false};
    if (auto serverEp =
          client.connect(jvs::net::IpAddress::ipv4_loopback(), 1234))
    {
      std::array<std::uint8_t, 64> receivedBytes{};
      if (auto receivedSize =
            client.recv(&receivedBytes[0], receivedBytes.size()))
      {
        std::string_view ss(
          jvs::net::alias_cast<const char*>(receivedBytes.data()),
          *receivedSize);
        if (ss == "Hello, world!\n")
        {
          if (auto sentSize = client.send("Hello, server!\n", 15))
          {
            std::cout << "Sent " << *sentSize << " bytes.\n";
            clientResult = true;
          }
          else
          {
            jvs::handle_all_errors(sentSize.take_error(), handleSocketError);
          }
        }
      }
      else
      {
        jvs::handle_all_errors(receivedSize.take_error(), handleSocketError);
      }

      client.close();
    }
    else
    {
      jvs::handle_all_errors(serverEp.take_error(), handleSocketError);
    }

    return clientResult;
  });

  auto waitTime = std::chrono::milliseconds(100);
  bool ready{false};
  std::future_status serverStatus;
  std::future_status clientStatus;
  do
  {
    serverStatus = serverTask.wait_for(waitTime);
    clientStatus = clientTask.wait_for(waitTime);
    ready = (serverStatus == std::future_status::ready &&
      clientStatus == std::future_status::ready);
  } while (!ready);

  EXPECT_TRUE((serverTask.get() && clientTask.get()));
}
