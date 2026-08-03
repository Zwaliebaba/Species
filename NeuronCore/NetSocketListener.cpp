// See header file for module description

#include "pch.h"

#include "NetLib.h"
#include "NetSocket.h"
#include "NetSocketListener.h"
#include "NetThread.h"
#include "NetUdpPacket.h"


NetSocketListener::NetSocketListener(unsigned short port)
{
  m_sockfd = NET_INVALID_SOCKET;
  m_port = port;
  m_listening = 0;
}


NetSocketListener::~NetSocketListener()
{
  // m_sockfd was left uninitialised until StartListening ran, so destroying a
  // listener that never started closed whatever the handle happened to be.
  if (m_sockfd != NET_INVALID_SOCKET)
  {
    NetCloseSocket(m_sockfd);
    m_sockfd = NET_INVALID_SOCKET;
  }
}


NetRetCode NetSocketListener::StartListening(NetCallBack functionPointer)
{
  NetRetCode ret = NetRetCode::NetOk;
  int bindAttempts = 0;
  NetIpAddress servaddr;
  NetIpAddress clientaddr;

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&clientaddr, 0, sizeof(clientaddr));

  // INVALID_SOCKET is ~0, not 0, so the old !m_sockfd test let a failed
  // socket() through and bound against an invalid handle.
  m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (m_sockfd == NET_INVALID_SOCKET)
  {
    NetDebugOut("Could not create listen socket: %d", NetGetLastError());
    return NetRetCode::NetFailed;
  }

  NetSocketHandle client = 0;
  unsigned int addrlen = sizeof(clientaddr);

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(m_port);

  // Make sure incoming arguments make sense
  if (functionPointer == (NetCallBack) nullptr)
  {
    return NetRetCode::NetBadArgs;
  }

  // Signal that we should be listening
  m_listening = 1;

  // Bind socket to port
  while ((bind(m_sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) == -1) && m_listening)
  {
    if ((bindAttempts++ == 10) || (!NetIsAddrInUse))
    {
      return NetRetCode::NetFailed;
    }
    else
    {
      NetDebugOut("Cannot bind to port");
      NetSleep(bindAttempts * 1000);
    }
  }

  int datasize = 0;
  char buf[MAX_PACKET_SIZE];
  NetSocketLenType cliLen = sizeof(clientaddr);

  while (m_listening)
  {
    memset(buf, 0, MAX_PACKET_SIZE * sizeof(char));
    datasize = recvfrom(m_sockfd, buf, MAX_PACKET_SIZE, 0, (struct sockaddr*)&clientaddr, &cliLen);

    // StopListening shuts the socket down to break us out of the recvfrom
    // above, so test before doing anything with what it returned.
    if (!m_listening)
      break;

    // A failed receive leaves datasize at -1, which used to go straight into
    // the packet, where MIN(-1, MAX_PACKET_SIZE) picked -1 and turned the
    // memcpy into one of SIZE_MAX bytes. Neither an oversized datagram nor
    // the stale ICMP unreachable Windows reports on a bound UDP socket is a
    // reason to stop listening, so those two just cost us the packet.
    if (NetIsSocketError(datasize))
    {
      int err = NetGetLastError();
      if (NetIsMsgTruncated(err) || NetIsReset(err))
        continue;

      NetDebugOut("Listener receive failed: %d", err);
      return NetRetCode::NetFailed;
    }

    // Call function pointer with datagram data (type is NetUdpPacket) -
    // the function pointed to must free NetUdpPacket passed in
    (*functionPointer)(new NetUdpPacket(client, &clientaddr, buf, datasize));
  }

  return ret;
}


void NetSocketListener::StopListening()
{
  m_listening = 0;
  NetSleep(250);
  if (m_sockfd != NET_INVALID_SOCKET)
  {
    shutdown(m_sockfd, 0);
  }
}
