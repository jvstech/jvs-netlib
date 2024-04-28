#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include <jvs-netlib/ip_address.h>
#include <jvs-netlib/ip_end_point.h>
#include <jvs-netlib/network_integers.h>
#include <jvs-netlib/socket.h>
#include <jvs-netlib/transport_end_point.h>

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

// Helpers for creating client and server sockets.

static auto HandleSocketError = [](const jvs::net::SocketError& e)
{
  std::cout.flush();
  e.log(std::cerr);
  std::cerr << "\n";
};

template <typename... TaskTs>
static void waitForAll(TaskTs&... tasks) noexcept
{
  bool ready{false};
  auto waitTime = std::chrono::milliseconds(100);
  
  do
  {
    ready = ((tasks.wait_for(waitTime) == std::future_status::ready) && ...);
  } while (!ready);
}

template <typename HandlerT, typename BeforeRecvHandlerT>
static bool createServerTask(const jvs::net::TransportEndPoint& ep,
  std::size_t bufferSize, const HandlerT& onReceived,
  const BeforeRecvHandlerT& onBeforeRecv) noexcept
{
  bool result{false};
  jvs::net::Socket server(ep.address().family(), ep.transport());
  auto boundEp = server.bind(ep.port());
  if (boundEp)
  {
    auto listenEp = server.listen();
    if (!listenEp && !listenEp.error_is_a<jvs::net::UnsupportedError>())
    {
      jvs::handle_all_errors(listenEp.take_error(), HandleSocketError);
      return false;
    }

    jvs::net::Socket* client{nullptr};
    auto connection = server.accept();
    if (!connection)
    {
      if (connection.error_is_a<jvs::net::UnsupportedError>())
      {
        jvs::consume_error(connection.take_error());
        client = &server;
      }
      else
      {
        jvs::handle_all_errors(connection.take_error(), HandleSocketError);
        return false;
      }
    }
    else
    {
      client = &*connection;
    }

    onBeforeRecv();

    std::vector<std::uint8_t> buffer(bufferSize);
    std::size_t bytesReceived;
    if (ep.transport() == jvs::net::Socket::Transport::Udp)
    {
      auto br = client->recvfrom(&buffer[0], buffer.size());
      if (!br)
      {
        jvs::handle_all_errors(br.take_error(), HandleSocketError);
        client->close();
        if (client != &server)
        {
          server.close();
        }

        return false;
      }

      bytesReceived = br->first;
    }
    else
    {
      auto br = client->recv(&buffer[0], buffer.size());
      if (!br)
      {
        jvs::handle_all_errors(br.take_error(), HandleSocketError);
        client->close();
        if (client != &server)
        {
          server.close();
        }

        return false;
      }

      bytesReceived = *br;
    }

    result = onReceived(*client, buffer, bytesReceived);
    if (client != &server)
    {
      client->close();
    }
  }
  else
  {
    jvs::handle_all_errors(boundEp.take_error(), HandleSocketError);
  }
  
  server.close();
  return result;
}

template <typename HandlerT, typename BeforeRecvHandlerT>
static bool createServerTask(std::string_view ep, std::size_t bufferSize,
  const HandlerT& onReceived,
  const BeforeRecvHandlerT& onBeforeRecv) noexcept
{
  auto serverEp = jvs::net::TransportEndPoint::parse(ep);
  if (!serverEp)
  {
    return false;
  }

  return createServerTask(*serverEp, bufferSize, onReceived, onBeforeRecv);
}

template <typename HandlerT>
static bool createServerTask(const jvs::net::TransportEndPoint& ep,
  std::size_t bufferSize, const HandlerT& onReceived) noexcept
{
  return createServerTask(ep, bufferSize, onReceived, [] {});
}

template <typename HandlerT>
static bool createServerTask(std::string_view ep, std::size_t bufferSize,
  const HandlerT& onReceived) noexcept
{
  auto serverEp = jvs::net::TransportEndPoint::parse(ep);
  if (!serverEp)
  {
    return false;
  }

  return createServerTask(*serverEp, bufferSize, onReceived, []{});
}

template <typename HandlerT>
static bool createClientTask(const jvs::net::TransportEndPoint& ep,
  const HandlerT& onConnected) noexcept
{
  jvs::net::Socket client(ep.address().family(), ep.transport());
  auto remoteEp = client.connect(ep.ip_end_point());
  if (!remoteEp)
  {
    jvs::handle_all_errors(remoteEp.take_error(), HandleSocketError);
    return false;
  }

  bool clientResult = onConnected(client, *remoteEp);

  client.close();
  return clientResult;
}

template <typename HandlerT>
static bool createClientTask(
  std::string_view ep, const HandlerT& onConnected) noexcept
{
  auto remoteEp = jvs::net::TransportEndPoint::parse(ep);
  if (!remoteEp)
  {
    return false;
  }

  return createClientTask(*remoteEp, onConnected);
}

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
    EXPECT_NE(addr, jvs::net::IpAddress::ipv6_any());
    auto ipver = jvs::convert_to<std::string>(addr.family());
    EXPECT_EQ(ipver, "IPv4");
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
      jvs::handle_all_errors(listenEndpoint.take_error(), HandleSocketError);
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
      jvs::handle_all_errors(listenEndpoint.take_error(), HandleSocketError);
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
  auto serverTask = std::async(std::launch::async, []
    {
      return createServerTask(
        "0.0.0.0:1234/tcp", /*bufferSize*/ 4096,
        [](jvs::net::Socket& client, std::vector<std::uint8_t>& buffer,
          std::size_t bytesReceived)
        {
          if (!bytesReceived)
          {
            return false;
          }

          std::string_view cs(
            jvs::net::alias_cast<const char*>(buffer.data()), bytesReceived);
          if (cs == "Hello, server!\n")
          {
            std::string_view cr("Hello, client!\n");
            auto bytesSent = client.send(cr.data(), cr.length());
            if (!bytesSent)
            {
              jvs::handle_all_errors(bytesSent.take_error(), HandleSocketError);
              return false;
            }

            return true;
          }

          return false;
        });
    });

  auto clientTask = std::async(std::launch::async, []
    {
      return createClientTask("127.0.0.1:1234/tcp",
        [](jvs::net::Socket& client, const jvs::net::IpEndPoint& ep)
        {
          std::string_view cs("Hello, server!\n");
          auto bytesSent = client.send(cs.data(), cs.size());
          if (!bytesSent)
          {
            jvs::handle_all_errors(bytesSent.take_error(), HandleSocketError);
            return false;
          }

          std::vector<std::uint8_t> buffer(4096);
          auto bytesReceived = client.recv(&buffer[0], buffer.size());
          if (!bytesReceived)
          {
            jvs::handle_all_errors(
              bytesReceived.take_error(), HandleSocketError);
            return false;
          }

          std::string_view cr(
            jvs::net::alias_cast<const char*>(buffer.data()), *bytesReceived);
          return (cr == "Hello, client!\n");
        });
    });

  waitForAll(serverTask, clientTask);
  EXPECT_TRUE((serverTask.get() && clientTask.get()));
}

TEST(SocketTest, BytesAvailable)
{
  auto serverTask = std::async(std::launch::async, []
    {
      return createServerTask("0.0.0.0:5678", /*bufferSize*/ 4096,
        [](jvs::net::Socket& client, std::vector<std::uint8_t>& buffer,
          std::size_t bytesReceived)
        {
          std::string_view cs("There should be 36 bytes available.\n");
          auto bytesSent = client.send(cs.data(), cs.length());
          if (!bytesSent)
          {
            jvs::handle_all_errors(bytesSent.take_error(), HandleSocketError);
            client.close();
            return false;
          }

          auto ignoredBytes = client.recv(&buffer[0], buffer.size());
          if (!ignoredBytes)
          {
            jvs::handle_all_errors(
              ignoredBytes.take_error(), HandleSocketError);
            client.close();
            return false;
          }

          return true;
        });
    });

  auto clientTask = std::async(std::launch::async, []
    {
      return createClientTask("127.0.0.1:5678",
        [](jvs::net::Socket& client, const jvs::net::IpEndPoint& remoteEp)
        {
          auto bytesSent = client.send("Hello\n", 6);
          if (!bytesSent)
          {
            jvs::handle_all_errors(bytesSent.take_error(), HandleSocketError);
            client.close();
            return false;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          auto bytesAvailable = client.available();
          if (!bytesAvailable)
          {
            jvs::handle_all_errors(
              bytesAvailable.take_error(), HandleSocketError);
            client.close();
            return false;
          }
           
          bool result = (*bytesAvailable == 36);
          bytesSent = client.send("Goodbye\n", 8);
          if (!bytesSent)
          {
            jvs::handle_all_errors(bytesSent.take_error(), HandleSocketError);
            client.close();
            return false;
          }

          return result;
        });
    });

  waitForAll(serverTask, clientTask);
  EXPECT_TRUE((serverTask.get() && clientTask.get()));
}

TEST(SocketTest, UdpIpv4)
{
  std::mutex socketMutex{};
  std::condition_variable socketVar{};
  bool serverReady = false;
  auto serverTask = std::async(std::launch::async, [&]
    {
      return createServerTask("0.0.0.0:7890/udp", /*bufferSize*/ 4096,
        [&](jvs::net::Socket& sock, std::vector<std::uint8_t>& buffer,
          std::size_t bytesReceived)
        {
          std::string_view cr(
            jvs::net::alias_cast<const char*>(buffer.data()), bytesReceived);
          if (cr != "Hello, UDP4 server\n")
          {
            sock.close();
            return false;
          }

          std::string_view cs("Hello, UDP4 client\n");
          if (!sock.remote())
          {
            std::cerr << "Unknown remote sender.\n";
            sock.close();
            return false;
          }

          auto bytesSent = sock.sendto(cs.data(), cs.length(), *sock.remote());
          if (!bytesSent)
          {
            jvs::handle_all_errors(bytesSent.take_error(), HandleSocketError);
            sock.close();
            return false;
          }

          sock.close();
          return true;
        },
        [&] {
          {
            std::lock_guard<std::mutex> serverReadyLock(socketMutex);
            serverReady = true;
          }

          socketVar.notify_one();
        });
    });

  auto clientTask = std::async(std::launch::async, [&]
    {
      return createClientTask("127.0.0.1:7890/udp",
        [&](jvs::net::Socket& sock, const jvs::net::IpEndPoint& remoteEp)
        {
          {
            std::unique_lock<std::mutex> clientLock(socketMutex);
            socketVar.wait(clientLock, [&] {return serverReady; });
          }
          
          std::string_view cs("Hello, UDP4 server\n");
          auto bytesSent = sock.send(cs.data(), cs.length());
          if (!bytesSent)
          {
            jvs::handle_all_errors(bytesSent.take_error(), HandleSocketError);
            sock.close();
            return false;
          }

          std::vector<std::uint8_t> buffer(4096);
          auto bytesReceived = sock.recv(&buffer[0], buffer.size());
          if (!bytesReceived)
          {
            jvs::handle_all_errors(
              bytesReceived.take_error(), HandleSocketError);
            sock.close();
            return false;
          }

          std::string_view cr(
            jvs::net::alias_cast<const char*>(buffer.data()), *bytesReceived);
          bool result = (cr == "Hello, UDP4 client\n");
          sock.close();
          return result;
        });
    });

  waitForAll(serverTask, clientTask);
  EXPECT_TRUE(serverTask.get() && clientTask.get());
}
