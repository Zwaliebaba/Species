#include "pch.h"


#include <string.h>

#include "NetLib.h"
#include "NetSocket.h"

#include "App.h"
#include "DebugUtils.h"
#include "ServerToClient.h"



ServerToClient::ServerToClient( char *_ip )
:   m_socket(NULL)
{
    strcpy ( m_ip, _ip );

    if( !g_app->m_bypassNetworking )
    {
        m_socket = new NetSocket();
        NetRetCode retCode = m_socket->Connect( _ip, 4001 );
        DarwiniaDebugAssert( retCode == NetOk );
    }

    m_lastKnownSequenceId = -1;
}


char *ServerToClient::GetIP()
{
    return m_ip;
}


NetSocket *ServerToClient::GetSocket()
{
    return m_socket;
}
