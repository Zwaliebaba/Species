#pragma once


#include <winsock2.h>
#include <windows.h>


class NetUdpPacket;


typedef unsigned long (WINAPI *NetCallBack)(NetUdpPacket *data);
typedef unsigned long (WINAPI *NetThreadFunc)(void *ptr);


// Define portable names for Win32 functions
#define NetGetLastError() 			WSAGetLastError()
#define NetSleep(a) 				Sleep(a)
#define NetCloseSocket 				closesocket
// Provide a Win32 wrapper declaration for hostname resolution.
// Implemented in NetLibWin32.cpp using getaddrinfo().
HOSTENT* NetGetHostByName(const char* hostname);

// ioctlsocket's third argument points at the flag value; it is not the value.
// This used to be a macro passing the literal 1 as that pointer, so winsock
// dereferenced address 0x1 and faulted on the first socket ever opened.
inline int NetSetSocketNonBlocking(SOCKET _socket)
{
  unsigned long nonBlocking = 1;
  return ioctlsocket(_socket, FIONBIO, &nonBlocking);
}

// Define portable names for Win32 types
#define NetSocketLenType 			int
#define NetSocketHandle 			SOCKET
#define NetHostDetails				HOSTENT
#define NetThreadHandle 			HANDLE
#define NetCallBackRetType			unsigned long WINAPI
#define NetPollObject 				struct fd_set
#define NetMutexHandle 				CRITICAL_SECTION

// Define portable names for Win32 constants
#define NET_SOCKET_ERROR 			SOCKET_ERROR
#define NET_INVALID_SOCKET INVALID_SOCKET
#define NCSD_SEND 					SD_SEND
#define NCSD_READ 					SD_RECEIVE
#define NET_RECEIVE_FLAG 			0

// Define portable ways to test various conditions
#define NetIsAddrInUse 				(WSAGetLastError() == WSAEADDRINUSE)
#define NetIsSocketError(a) 		(a == SOCKET_ERROR)
#define NetIsBlockingError(a) 		((a == WSAEALREADY) || (a == WSAEWOULDBLOCK) || (a == WSAEINVAL))
#define NetIsConnected(a) 			(a == WSAEISCONN)
#define NetIsReset(a) 				((a == WSAECONNRESET) || (a == WSAESHUTDOWN))
#define NetIsMsgTruncated(a) (a == WSAEMSGSIZE)


