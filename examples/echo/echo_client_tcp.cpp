//!
//! @file echo_client_tcp.cpp
//! 
//! Example usage of the Socket class to act as an echo protocol client via TCP.
//! 

#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "socket.h"

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

static auto errHandler = [](const ErrorInfoBase& e) { reportError(e); };

// Socket stream-insertion operator
Socket& operator<<(Socket& s, const std::string& data) noexcept
{
  auto bytesSent = s.send(data.data(), data.length());
  if (!bytesSent)
  {
    handle_all_errors(bytesSent.take_error(), errHandler);
  }

  std::size_t totalSent = *bytesSent;
  while (totalSent < data.length())
  {
    bytesSent = s.send(data.data() + totalSent, data.length() - totalSent);
    if (!bytesSent)
    {
      handle_all_errors(bytesSent.take_error(),  errHandler);
    }

    totalSent += *bytesSent;
  }

  return s;
}

// Socket stream-extraction operator
Socket& operator>>(Socket& s, std::string& data) noexcept
{
  std::array<char, 4096> buffer;
  Expected<std::size_t> bytesRead = 0;
  auto doRead = [&]
  {
    if (!bytesRead)
    {
      handle_all_errors(bytesRead.take_error(), errHandler);
    }

    bytesRead = s.recv(&buffer[0], buffer.size());
    if (!bytesRead)
    {
      handle_all_errors(bytesRead.take_error(), errHandler);
    }

    if (!*bytesRead)
    {
      // Connection was closed.
      return;
    }

    data.reserve(*bytesRead);
    data.append(buffer.data(), *bytesRead);
  };

  // FIXME: If the received data happens to be exactly the same size as the
  // buffer, this will block trying to read non-existant data after the first
  // amount of data has been read.
  do
  {
    doRead();
  } while (*bytesRead == buffer.size());
  
  return s;
}

int main(int argc, char** argv)
{
  std::string_view epStr = "127.0.0.1:7";
  if (argc > 1)
  {
    epStr = argv[1];
  }

  if (auto ep = IpEndPoint::parse(epStr))
  {
    Socket client(ep->address().family(), Socket::Transport::Tcp);
    auto remoteEp = client.connect(*ep);
    if (remoteEp)
    {
      std::string input;
      std::string reply;
      while (std::getline(std::cin, input))
      {
        client << input;
        client >> reply;
        std::cout << reply << '\n';
      }

      client.close();
    }
    else
    {
      handle_all_errors(remoteEp.take_error(), errHandler);
    }
  }
  else
  {
    handle_all_errors(
      create_string_error("Unable to parse endpoint: ", epStr), errHandler);
  }

  return 0;
}
