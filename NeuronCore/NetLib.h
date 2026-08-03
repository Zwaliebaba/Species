// ****************************************************************************
//  Top level include file for NetLib
//
//  NetLib - A very thin portable UDP network library
// ****************************************************************************

#pragma once

#include "NetLibWin32.h"

#if (!defined MIN)
#define MIN(a, b) ((a < b) ? a : b)
#endif

void NetDebugOut(const char* fmt, ...);

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
