// See header file for description of this library

#include "pch.h"

#include <stdio.h>

#include <string>

#include "NetLib.h"


void NetDebugOutMessage(std::string_view _message)
{
  // string_view is not guaranteed null-terminated and both sinks want a C
  // string, so this copies once. The char[512] it replaces was written
  // through an unbounded vsprintf.
  const std::string message(_message);
#ifdef WIN32
  OutputDebugStringA(message.c_str());
#else
  fprintf(stderr, "%s\n", message.c_str());
#endif
}



// ****************************************************************************
// Class NetLib
// ****************************************************************************

NetLib::NetLib()
{
}


NetLib::~NetLib()
{
#ifdef WIN32
    WSACleanup();
#endif
}


bool NetLib::Initialise()
{
#ifdef WIN32
    WORD versionRequested;
    WSADATA wsaData;
    
    versionRequested = MAKEWORD(2, 2);
 
    if (WSAStartup(versionRequested, &wsaData))
    {
        NetDebugOut("WinSock startup failed");
        return false;
    }
 
    // Confirm that the WinSock DLL supports 2.2. Note that if the DLL supports
	// versions greater than 2.2 in addition to 2.2, it will still return
    // 2.2 in wVersion since that is the version we requested.                                       
    if ((LOBYTE(wsaData.wVersion) != 2) || (HIBYTE(wsaData.wVersion) != 2))
    {
        // Tell the user that we could not find a usable WinSock DLL
        WSACleanup();
        NetDebugOut("No valid WinSock DLL found");
        return false; 
    }
#endif

    return true;
}
