// ****************************************************************************
//  What is left of NetLib.
//
//  It described itself as "a very thin portable UDP network library": a socket,
//  a listener, a datagram, a thread, a mutex and a sheet of Win32 macros
//  standing in for a portability layer that only ever had one platform.
//  network-transport T4 and T5 replaced the whole of it with UdpSocket, and T6
//  deleted it — taking with it a connect retry loop meaningless for UDP, a
//  partial-send loop that cannot happen for a datagram, a Flush() that handed a
//  SOCKET to _fdopen and had no callers, select timeout arithmetic off by a
//  factor of ten, and a global MIN macro.
//
//  This is what nothing else does: start Winsock, stop Winsock, and write a
//  line to the debugger.
// ****************************************************************************

#pragma once

#include <format>
#include <string_view>
#include <utility>

// Writes one already-formatted line to the platform's debug output. Callers
// want NetDebugOut below; this is separate only so the platform #ifdef stays
// in the .cpp rather than being dragged into every translation unit that
// includes this header.
void NetDebugOutMessage(std::string_view _message);

// Was `void NetDebugOut(const char* fmt, ...)` over a char[512] and an
// unbounded vsprintf. strings-modernised T18. The format string is checked at
// compile time now, which is worth having here specifically: every call site is
// a literal, and several interpolate an errno-style value that used to be
// spelled %d with nothing verifying it.
template <class... Types> void NetDebugOut(std::format_string<Types...> _fmt, Types&&... _args)
{
  NetDebugOutMessage(std::format(_fmt, std::forward<Types>(_args)...));
}

class NetLib
{
  public:
    NetLib();
    ~NetLib();

    bool Initialise(); // Returns false on failure
};
