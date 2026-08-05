// ****************************************************************************
//  Top level include file for NetLib
//
//  NetLib - A very thin portable UDP network library
// ****************************************************************************

#pragma once

#include <format>
#include <string_view>
#include <utility>

#include "NetLibWin32.h"

#if (!defined MIN)
#define MIN(a, b) ((a < b) ? a : b)
#endif

// Writes one already-formatted line to the platform's debug output. Callers
// want NetDebugOut below; this is separate only so the platform #ifdef stays
// in the .cpp rather than being dragged into every translation unit that
// includes this header.
void NetDebugOutMessage(std::string_view _message);

// Was `void NetDebugOut(const char* fmt, ...)` over a char[512] and an
// unbounded vsprintf. strings-modernised T18. The format string is checked at
// compile time now, which is worth having here specifically: every one of the
// fourteen call sites is a literal, and six of them interpolate an errno-style
// int that used to be spelled %d with nothing verifying it.
template <class... Types> void NetDebugOut(std::format_string<Types...> _fmt, Types&&... _args)
{
  NetDebugOutMessage(std::format(_fmt, std::forward<Types>(_args)...));
}

#define MAX_HOSTNAME_LEN 256
#define MAX_PACKET_SIZE 512

using NetIpAddress = struct sockaddr_in;

// int rather than the default, because NetFailed is -1. This one does NOT cross
// the wire — it is a return code — which is why it has no pinning test, unlike
// NetworkUpdate::UpdateType and ServerToClientLetter::LetterType.
enum class NetRetCode : int
{
  NetFailed = -1,
  NetOk,
  NetTimedout,
  NetBadArgs,
  NetMoreData,
  NetClientDisconnect,
  NetNotSupported
};

class NetLib
{
  public:
    NetLib();
    ~NetLib();

    bool Initialise(); // Returns false on failure
};
