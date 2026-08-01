// See header file for module description

#include "pch.h"

#include <string.h>		// For memcpy

#include "NetUdpPacket.h"


NetUdpPacket::NetUdpPacket(int sockfd, NetIpAddress *clientAddress, char *buf, int len)
:	m_sockfd(sockfd)
{
	m_length = MIN(len, MAX_PACKET_SIZE);
	memcpy(&m_clientAddress, clientAddress, sizeof(NetIpAddress));
	memcpy(m_data, buf, m_length * sizeof(char));
}
