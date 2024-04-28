///
/// @file socket_errors.h
/// 
/// Contains declarations for socket-specific errors and platform-independent
/// error constants.
/// 

#if !defined(JVS_NETLIB_SOCKET_ERRORS_H_)
#define JVS_NETLIB_SOCKET_ERRORS_H_

#include "native_sockets.h"

#include "error.h"

namespace jvs::net
{

class SocketError : public ErrorInfo<SocketError>
{
  int code_;
  std::string message_;
public:

  static char ID;

  SocketError(int code, const std::string& message);
  SocketError(int code);
  virtual ~SocketError() = default;

  virtual bool is_fatal() const override;
  int code() const noexcept;
  virtual void log(std::ostream& os) const override;
};

struct SocketErrorNonFatal : ErrorInfo<SocketErrorNonFatal, SocketError>
{
  static char ID;
  using Base::Base;
  virtual ~SocketErrorNonFatal() = default;

  bool is_fatal() const override;
};

struct AddressInfoError : ErrorInfo<AddressInfoError, SocketError>
{
  static char ID;
  AddressInfoError(int code);
};

struct NonBlockingStatus final
  : ErrorInfo<NonBlockingStatus, SocketErrorNonFatal>
{
  static char ID;
  using Base::Base;  
};

struct UnsupportedError final : ErrorInfo<UnsupportedError, SocketError>
{
  static char ID;
  using Base::Base;
};

// This namespace doesn't necessarily contain and exhaustive list of every
// socket error type, but enough to hopefully be useful.
namespace errcodes
{

#if defined(_WIN32)
#define SOCK_ERR_C(bsd, win) (win)
#else
#define SOCK_ERR_C(bsd, win) (bsd)
#endif

inline constexpr int EWouldBlock = SOCK_ERR_C(EWOULDBLOCK, WSAEWOULDBLOCK);
inline constexpr int EAgain = SOCK_ERR_C(EAGAIN, WSAEWOULDBLOCK);
inline constexpr int EInProgress = SOCK_ERR_C(EINPROGRESS, WSAEINPROGRESS);
inline constexpr int EAlready = SOCK_ERR_C(EALREADY, WSAEALREADY);
inline constexpr int ENotSock = SOCK_ERR_C(ENOTSOCK, WSAENOTSOCK);
inline constexpr int EDestAddrReq = SOCK_ERR_C(EDESTADDRREQ, WSAEDESTADDRREQ);
inline constexpr int EMsgSize = SOCK_ERR_C(EMSGSIZE, WSAEMSGSIZE);
inline constexpr int EProtoType = SOCK_ERR_C(EPROTOTYPE, WSAEPROTOTYPE);
inline constexpr int ENoProtoOpt = SOCK_ERR_C(ENOPROTOOPT, WSAENOPROTOOPT);
inline constexpr int EProtoNoSupport =
  SOCK_ERR_C(EPROTONOSUPPORT, WSAEPROTONOSUPPORT);
inline constexpr int ESockTNoSupport =
  SOCK_ERR_C(ESOCKTNOSUPPORT, WSAESOCKTNOSUPPORT);
inline constexpr int EOpNotSupp = SOCK_ERR_C(EOPNOTSUPP, WSAEOPNOTSUPP);
inline constexpr int EPFNoSupport = SOCK_ERR_C(EPFNOSUPPORT, WSAEPFNOSUPPORT);
inline constexpr int EAFNoSupport = SOCK_ERR_C(EAFNOSUPPORT, WSAEAFNOSUPPORT);
inline constexpr int EAddrInUse = SOCK_ERR_C(EADDRINUSE, WSAEADDRINUSE);
inline constexpr int EAddrNotAvail =
  SOCK_ERR_C(EADDRNOTAVAIL, WSAEADDRNOTAVAIL);
inline constexpr int ENetDown = SOCK_ERR_C(ENETDOWN, WSAENETDOWN);
inline constexpr int ENetUnreach = SOCK_ERR_C(ENETUNREACH, WSAENETUNREACH);
inline constexpr int ENetReset = SOCK_ERR_C(ENETRESET, WSAENETRESET);
inline constexpr int EConnAborted = SOCK_ERR_C(ECONNABORTED, WSAECONNABORTED);
inline constexpr int EConnReset = SOCK_ERR_C(ECONNRESET, WSAECONNRESET);
inline constexpr int ENoBufs = SOCK_ERR_C(ENOBUFS, WSAENOBUFS);
inline constexpr int EIsConn = SOCK_ERR_C(EISCONN, WSAEISCONN);
inline constexpr int ENotConn = SOCK_ERR_C(ENOTCONN, WSAENOTCONN);
inline constexpr int EShutdown = SOCK_ERR_C(ESHUTDOWN, WSAESHUTDOWN);
inline constexpr int ETooManyRefs = SOCK_ERR_C(ETOOMANYREFS, WSAETOOMANYREFS);
inline constexpr int ETimedOut = SOCK_ERR_C(ETIMEDOUT, WSAETIMEDOUT);
inline constexpr int EConnRefused = SOCK_ERR_C(ECONNREFUSED, WSAECONNREFUSED);
inline constexpr int ELoop = SOCK_ERR_C(ELOOP, WSAELOOP);
inline constexpr int ENameTooLong = SOCK_ERR_C(ENAMETOOLONG, WSAENAMETOOLONG);
inline constexpr int EHostDown = SOCK_ERR_C(EHOSTDOWN, WSAEHOSTDOWN);
inline constexpr int EHostUnreach = SOCK_ERR_C(EHOSTUNREACH, WSAEHOSTUNREACH);
inline constexpr int ENotEmpty = SOCK_ERR_C(ENOTEMPTY, WSAENOTEMPTY);
inline constexpr int EUsers = SOCK_ERR_C(EUSERS, WSAEUSERS);
inline constexpr int EDQuot = SOCK_ERR_C(EDQUOT, WSAEDQUOT);
inline constexpr int EStale = SOCK_ERR_C(ESTALE, WSAESTALE);
inline constexpr int ERemote = SOCK_ERR_C(EREMOTE, WSAEREMOTE);

#undef SOCK_ERR_C

} // namespace errcodes

} // namespace jvs::net


#endif // !JVS_NETLIB_SOCKET_ERRORS_H_
