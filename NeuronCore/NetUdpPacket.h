#pragma once


// ****************************************************************************
//  An object containing a single UDP datagram
// ****************************************************************************

#include "NetLib.h"
#include "ProtocolLimits.h"


class NetUdpPacket
{
public:
	NetUdpPacket(int sockfd, NetIpAddress *clientaddr, char *buf, int len);
	
	int 			m_sockfd;
	int 			m_length;
	NetIpAddress	m_clientAddress;
  char m_data[MaxDatagramSize];
};


