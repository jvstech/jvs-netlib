///
/// @file socket_context.h
/// 
/// Contains the SocketContext class declaration.
/// 

#if !defined(JVS_NETLIB_SOCKET_CONTEXT_H_)
#define JVS_NETLIB_SOCKET_CONTEXT_H_

#include "native_sockets.h"

#include "utils.h"

namespace jvs::net
{

///
/// @class SocketContext
/// 
/// Wrapper around the native system's socket handle/file descriptor type.
/// 
class SocketContext final
{
public:
  using type = jvs::ReturnType<decltype(::socket)>;

  SocketContext(type socketHandleOrDescriptor);

  operator type() const noexcept;

  type value() const noexcept;

private:
  type value_;
};

} // namespace jvs::net


#endif // !JVS_NETLIB_SOCKET_CONTEXT_H_
