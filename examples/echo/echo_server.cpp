//!
//! @file echo_server.cpp
//! 
//! Example usage of the Socket class to act as an echo protocol server.
//! 

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "socket.h"
#include "transport_end_point.h"

#include "print_data.h"

using namespace jvs;
using namespace jvs::net;

static auto reportError = [](const jvs::ErrorInfoBase& e)
{
  std::cout.flush();
  e.log(std::cerr);
  std::cerr << '\n';
  std::exit(1);
};

static void handleClient(Socket& client)
{
  std::vector<std::uint8_t> buffer;
  for (;;)
  {
    char startBuf = 0;
    auto recvResult = client.recv(&startBuf, 0);
    if (!recvResult)
    {
      handle_all_errors(recvResult.take_error(), reportError);
    }

    auto bytesAvailable = client.available();
    if (!bytesAvailable)
    {
      handle_all_errors(bytesAvailable.take_error(), reportError);
    }

    if (*bytesAvailable > buffer.size())
    {
      buffer.resize(*bytesAvailable);
    }

    auto receivedSize = client.recv(&buffer[0], buffer.size());
    if (receivedSize)
    {
      if (*receivedSize)
      {
        std::cout << "Received " << *receivedSize << " bytes: \"";
        print_data(buffer.data(), buffer.data() + *receivedSize,
          std::cout);
        std::cout << "\"\n";
        auto sentBytes =
          client.send(buffer.data(), *receivedSize);
        if (!sentBytes)
        {
          consume_error(sentBytes.take_error());
          client.close();
          break;
        }

        std::cout << "Sent " << *sentBytes << " bytes back.\n";
      }
      else
      {
        // Remote end disconnected.
        std::cout << "Remote end disconnected.\n";
        client.close();
        break;
      }
    }
    else
    {
      handle_all_errors(
        receivedSize.take_error(), reportError);
    }
  }                
}

static void handleTcpClient(Socket&& client)
{
  handleClient(client);
}

int main(int argc, char** argv)
{
  if (argc <= 1)
  {
    handle_all_errors(create_string_error("Usage: ", argv[0],
      " <local-address>:<port>[/<tcp|udp>]\n"), reportError);
  }

  std::string_view localEpStr = argv[1];
  if (auto requestedEp = TransportEndPoint::parse(localEpStr))
  {
    bool isUdp = requestedEp->transport() == Socket::Transport::Udp;
    Socket server(requestedEp->address().family(), requestedEp->transport());
    auto boundEp = server.bind(requestedEp->ip_end_point());
    if (boundEp)
    {
      Expected<IpEndPoint> listenEp = [&]
      {
        if (isUdp)
        {
          return Expected<IpEndPoint>(requestedEp->ip_end_point());
        }
        else
        {
          return server.listen();
        }
      }();

      if (listenEp)
      {
        std::vector<std::future<void>> connections{};
        std::cout << "Listening on " << to_string(*listenEp) << ".\n";
        // Run forever (until the control handler is invoked).
        for (;;)
        {
          auto connection = server.accept();
          if (connection ||
            (!connection && connection.error_is_a<UnsupportedError>() &&
              requestedEp->transport() == Socket::Transport::Udp))
          {
            if (connection)
            {
              std::cout << "Received connection ("
                << to_string(connection->local()) << " <- "
                << to_string(*connection->remote()) << ")\n";
            }
            else
            {
              consume_error(connection.take_error());
            }

            if (!isUdp)
            {
              auto echoTask = std::async(std::launch::async,
                handleTcpClient, std::move(*connection));
              connections.push_back(std::move(echoTask));
            }
            else
            {
              handleClient(server);
            }
          }
          else
          {
            handle_all_errors(connection.take_error(), reportError);
          }

          for (std::size_t i = connections.size(); i-- > 0; )
          {
            auto status = connections[i].wait_for(std::chrono::milliseconds(5));
            if (status == std::future_status::ready)
            {
              connections.erase(connections.begin() + i);
            }
          }

          if (connections.empty())
          {
            break;
          }
        }
      }
      else
      {
        handle_all_errors(listenEp.take_error(), reportError);
      }
    }
    else
    {
      handle_all_errors(boundEp.take_error(), reportError);
    }
  }
  else
  {
    handle_all_errors(
      create_string_error("Unable to parse endpoint: ", localEpStr),
      reportError);
  }

  return 0;
}
