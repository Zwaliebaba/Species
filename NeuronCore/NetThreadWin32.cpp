// See net_thread.h for description of module

#include "pch.h"

#include "NetThread.h"


NetRetCode NetStartThread(NetThreadFunc functionPtr)
{
	NetRetCode retVal = NetOk;
	DWORD dwID = 0;
	
	if (CreateThread(nullptr, nullptr, functionPtr, nullptr, nullptr, &dwID) == nullptr)
	{
		NetDebugOut("Thread creation failed");
		retVal = NetFailed;
	}

	return retVal;
}
