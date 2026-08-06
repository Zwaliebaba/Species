// See header file for module description

#include "pch.h"

#include <string.h>		// For memcpy

#include "NetUdpPacket.h"


NetUdpPacket::NetUdpPacket(int sockfd, NetIpAddress *clientAddress, char *buf, int len)
:	m_sockfd(sockfd)
{
  // MIN is signed, so a negative length passes straight through and becomes an
  // enormous size_t at the memcpy. Callers are not supposed to hand us one,
  // but the cost of surviving it is a comparison.
  m_length = MIN(len, MaxDatagramSize);
  if (m_length < 0)
    m_length = 0;

  memcpy(&m_clientAddress, clientAddress, sizeof(NetIpAddress));
	memcpy(m_data, buf, m_length * sizeof(char));
}
