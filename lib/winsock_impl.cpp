#include "winsock_impl.h"

#include <string>
#include <tuple>

#include "native_sockets.h"

#include "socket.h"

#include "socket_impl.h"
#include "utils.h"

using namespace jvs::net;
using Family = IpAddress::Family;
using Transport = Socket::Transport;

namespace 
{

// Winsock versions
static constexpr WORD WinsockVersions[] = {
  MAKEWORD(2, 2),
  MAKEWORD(2, 1),
  MAKEWORD(2, 0),
  MAKEWORD(1, 1),
  MAKEWORD(1, 0)
};

static std::string getWinsockErrorMessage(int ecode) noexcept
{
  LPWSTR errorText = nullptr;
  DWORD charCount = ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS |
    FORMAT_MESSAGE_MAX_WIDTH_MASK,
    /*lpSource*/ nullptr, static_cast<DWORD>(ecode),
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    reinterpret_cast<LPWSTR>(&errorText), /*nSize*/ 0,
    /*Arguments*/ nullptr);
  if (charCount && errorText)
  {
    int sizeNeeded = ::WideCharToMultiByte(CP_UTF8, 0, errorText,
      static_cast<int>(charCount), /*lpMultiByteStr*/ nullptr,
      /*cbMultiByte*/ 0, /*lpDefaultChar*/ nullptr,
      /*lpUsedDefaultChar*/ nullptr);
    std::string utf8ErrMessage(sizeNeeded, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, errorText, static_cast<int>(charCount),
      &utf8ErrMessage[0], sizeNeeded, /*lpDefaultChar*/ nullptr,
      /*lpUsedDefaultChar*/ nullptr);
    ::LocalFree(errorText);
    return utf8ErrMessage;
  }

  return "Error code " + std::to_string(ecode);
}

// Most nonzero error codes returned by the getaddrinfo function map to the set
// of errors outlined by Internet Engineering Task Force (IETF) recommendations.
// The gai_strerror function is provided for compliance with IETF
// recommendations, but it is not thread safe. Therefore, use of traditional
// Windows Sockets functions such as WSAGetLastError is recommended.
static int translateWinsockAddrInfoError(int ecode) noexcept
{
  switch (ecode)
  {
  case EAI_AGAIN:
    return static_cast<int>(WSATRY_AGAIN);
  case EAI_BADFLAGS:
    return static_cast<int>(WSAEINVAL);
  case EAI_FAIL:
    return static_cast<int>(WSANO_RECOVERY);
  case EAI_FAMILY:
    return static_cast<int>(WSAEAFNOSUPPORT);
  case EAI_MEMORY:
    return static_cast<int>(WSA_NOT_ENOUGH_MEMORY);
  case EAI_NONAME:
    return static_cast<int>(WSAHOST_NOT_FOUND);
  case EAI_SERVICE:
    return static_cast<int>(WSATYPE_NOT_FOUND);
  case EAI_SOCKTYPE:
    return static_cast<int>(WSAESOCKTNOSUPPORT);
  default:
    return ecode;
  }
}

static std::string getWinsockAddrInfoErrorMessage(int ecode) noexcept
{
  return getWinsockErrorMessage(translateWinsockAddrInfoError(ecode));
}

static auto initWinsock() noexcept
{
  struct { int Code{WSAVERNOTSUPPORTED}; WSADATA Data; } result;
  // Try to initialize the Winsock version in descending order.
  for (int i = 0; i < (sizeof(WinsockVersions) / sizeof(WORD)) &&
    result.Code == WSAVERNOTSUPPORTED; ++i)
  {
    result.Code = ::WSAStartup(WinsockVersions[i], &result.Data);
  }

  return result;
}

} // namespace

Socket::SocketImpl::SocketImpl()
{
  auto initResult = initWinsock();
  startup_code_ = initResult.Code;
  data_ = initResult.Data;
}

Socket::SocketImpl::SocketImpl(SocketContext ctx)
  : socket_info_(ctx)
{
  auto initResult = initWinsock();
  startup_code_ = initResult.Code;
  data_ = initResult.Data;
}

Socket::SocketImpl::~SocketImpl()
{
  if (!startup_code_)
  {
    ::WSACleanup();
  }
}

const std::optional<IpEndPoint>& Socket::SocketImpl::local_endpoint()
  const noexcept
{
  return local_endpoint_;
}

const std::optional<IpEndPoint>& Socket::SocketImpl::remote_endpoint()
  const noexcept
{
  return remote_endpoint_;
}

Socket::SocketImpl::SocketImpl(
  Family addressFamily, Transport transport)
{
  auto initResult = initWinsock();
  startup_code_ = initResult.Code;
  data_ = initResult.Data;
  if (!startup_code_)
  {
    if (auto si = create_socket(addressFamily, transport))
    {
      socket_info_ = *si;
    }
    else
    {
      jvs::consume_error(si.take_error());
    }
  }
}

int jvs::net::get_last_error() noexcept
{
  return ::WSAGetLastError();
}

std::string jvs::net::get_socket_error_message(int ecode) noexcept
{
  return getWinsockErrorMessage(ecode);
}

std::string jvs::net::get_addrinfo_error_message(int ecode) noexcept
{
  return getWinsockAddrInfoErrorMessage(ecode);
}

// Winsock-specific Socket member implementations
////////////////////////////////////////////////////////////////////////////////

jvs::Expected<std::size_t> Socket::available() noexcept
{
  auto ctx = impl_->socket_info_.context();
  u_long bytesAvailable = 0;
  int result = ::ioctlsocket(ctx, FIONREAD, &bytesAvailable);
  if (is_error_result(result))
  {
    return jvs::net::create_socket_error(ctx);
  }

  return static_cast<std::size_t>(bytesAvailable);
}

int Socket::close() noexcept
{
  // This function (closesocket) is why Socket::close() can't be implemented
  // as a common function. Windows socket handles are kernel objects -- not
  // file descriptors. As such, `close()` can't be used.
  int closeResult = ::closesocket(impl_->socket_info_.context());
  impl_->socket_info_.reset();
  impl_->local_endpoint_.reset();
  impl_->remote_endpoint_.reset();
  return closeResult;
}