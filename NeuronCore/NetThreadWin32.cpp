// See net_thread.h for description of module

#include "pch.h"

#include "NetThread.h"


NetRetCode NetStartThread(NetThreadFunc functionPtr)
{
  NetRetCode retVal = NetOk;
  DWORD dwID = 0;

  if (CreateThread(nullptr, 0, functionPtr, nullptr, 0, &dwID) == nullptr)
  {
    NetDebugOut("Thread creation failed");
    retVal = NetFailed;
  }

  return retVal;
}
