#include "socket.h"

#include <cstdint>

#define WIN32_LEAN_AND_MEAN
// #TODO: maybe #define VC_EXTRALEAN?
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "error.h"

namespace
{

using WinsockBind = decltype(::bind);
using WinsockClose = decltype(::closesocket);

struct WinsockApi final
{
  WinsockApi(WORD versionRequested)
  {
    startup_error = ::WSAStartup(versionRequested, &impl_data);
  }

  ~WinsockApi()
  {
    ::WSACleanup();
  }

  WSADATA impl_data{};
  int startup_error{0};

  WinsockBind* bind_native{nullptr};
  WinsockClose* close_native{nullptr};

};

inline constexpr WORD WinsockVersion = MAKEWORD(2, 2);

static jvs::Expected<WinsockApi&> get_api()
{
  static WinsockApi* api = new WinsockApi(WinsockVersion);
  return *api;
}

} // namespace `anonymous-namespace'

namespace jvs::net
{

class Socket::SocketImpl
{
  SocketInfo socket_info_{};

};

} // namespace jvs::net

void jvs::net::Socket::close()
{
  get_api().close_native(handle_);
}
