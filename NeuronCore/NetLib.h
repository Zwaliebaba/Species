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

enum NetRetCode
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
