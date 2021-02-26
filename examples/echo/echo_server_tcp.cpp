//!
//! @file echo_server_tcp.cpp
//! 
//! Example usage of the Socket class to act as an echo protocol server over
//! TCP.
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

#include "print_data.h"

using namespace jvs;
using namespace jvs::net;

template <typename ErrT>
static void reportError(const ErrT& e) noexcept
{
  std::cerr << "Error: ";
  e.log(std::cerr);
  std::cerr << '\n';
  std::exit(1);
}

int main(int argc, char** argv)
{
  std::string_view localEpStr = "0.0.0.0:7";
  if (argc > 1)
  {
    localEpStr = argv[1];
  }

  auto errHandler = [](const ErrorInfoBase& e)
  {
    reportError(e);
  };

  if (auto requestedEp = IpEndPoint::parse(localEpStr))
  {
    Socket server(requestedEp->address().family(), Socket::Transport::Tcp);
    auto boundEp = server.bind(*requestedEp);
    if (boundEp)
    {
      auto listenEp = server.listen();
      if (listenEp)
      {
        std::vector<std::future<void>> connections{};
        std::cout << "Listening on " << to_string(*listenEp) << ".\n";
        // Run forever (until the control handler is invoked).
        for (;;)
        {
          auto connection = server.accept();
          if (connection)
          {
            std::cout << "Received connection ("
                      << to_string(connection->local()) << " <- "
                      << to_string(*connection->remote()) << ")\n";
            auto echoTask = std::async(std::launch::async, 
              [&](Socket&& client)
              {
                std::vector<std::uint8_t> buffer(4096, 0);
                for (;;)
                {
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
                      receivedSize.take_error(), errHandler);
                  }
                }                
              }, std::move(*connection));
            connections.push_back(std::move(echoTask));
          }
          else
          {
            handle_all_errors(connection.take_error(), errHandler);
          }
        }
      }
      else
      {
        handle_all_errors(listenEp.take_error(), errHandler);
      }
    }
    else
    {
      handle_all_errors(boundEp.take_error(), errHandler);
    }
  }
  else
  {
    handle_all_errors(
      create_string_error("Unable to parse endpoint: ", localEpStr),
      errHandler);
  }

  return 0;
}
