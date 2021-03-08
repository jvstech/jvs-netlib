#if !defined(JVS_NETLIB_NATIVE_SOCKETS_H_)
#define JVS_NETLIB_NATIVE_SOCKETS_H_

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define VC_EXTRALEAN
#  define NOMINMAX
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <netdb.h>
#  include <unistd.h>
#endif

#endif // !JVS_NETLIB_NATIVE_SOCKETS_H_
