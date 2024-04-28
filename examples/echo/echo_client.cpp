///
/// @file echo_client.cpp
/// 
/// Example usage of the Socket class to act as an echo protocol client.
/// 

#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <jvs-netlib/error.h>
#include <jvs-netlib/socket.h>
#include <jvs-netlib/transport_end_point.h>

using namespace jvs;
using namespace jvs::net;

namespace
{

void reportError(const jvs::ErrorInfoBase& e)
{
  std::cout.flush();
  e.log(std::cerr);
  std::cerr << '\n';
  std::exit(1);
}

}  // namespace

// Socket stream-insertion operator
Socket& operator<<(Socket& s, const std::string& data) noexcept
{
  auto bytesSent = s.send(data.data(), data.length());
  if (!bytesSent)
  {
    handle_all_errors(bytesSent.take_error(), reportError);
  }

  std::size_t totalSent = *bytesSent;
  while (totalSent < data.length())
  {
    bytesSent = s.send(data.data() + totalSent, data.length() - totalSent);
    if (!bytesSent)
    {
      handle_all_errors(bytesSent.take_error(),  reportError);
    }

    totalSent += *bytesSent;
  }

  return s;
}

// Socket stream-extraction operator
Socket& operator>>(Socket& s, std::string& data)
{
  char startBuf = 0;
  auto recvResult = s.recv(&startBuf, 0);
  if (!recvResult)
  {
    handle_all_errors(recvResult.take_error(), reportError);
  }

  auto bytesToRead = s.available();
  if (!bytesToRead)
  {
    handle_all_errors(bytesToRead.take_error(), reportError);
  }

  std::vector<char> buffer(*bytesToRead);
  auto bytesRead = s.recv(&buffer[0], buffer.size());
  if (!bytesRead)
  {
    handle_all_errors(bytesRead.take_error(), reportError);
  }

  if (!*bytesRead)
  {
    // Connection was closed.
    return s;
  }

  data.reserve(*bytesRead);
  data.append(buffer.data(), *bytesRead);
  return s;
}

int main(int argc, char** argv)
{
  if (argc <= 1)
  {
    handle_all_errors(create_string_error(
      "Usage: ", argv[0], " <address>:<port>[/<tcp|udp>]\n"), reportError);
  }

  std::string_view epStr = argv[1];
  if (auto ep = TransportEndPoint::parse(epStr))
  {
    Socket client(ep->address().family(), ep->transport());
    auto remoteEp = client.connect(ep->ip_end_point());
    if (remoteEp)
    {
      std::string input;
      std::string reply;
      while (std::getline(std::cin, input))
      {
        client << input;
        client >> reply;
        std::cout << reply << '\n';
        reply.clear();
      }

      client.close();
    }
    else
    {
      handle_all_errors(remoteEp.take_error(), reportError);
    }
  }
  else
  {
    handle_all_errors(
      create_string_error("Unable to parse endpoint: ", epStr), reportError);
  }

  return 0;
}
