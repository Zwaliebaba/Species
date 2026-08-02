#include "pch.h"

#include "NetLib.h"
#include "NetMutex.h"
#include "NetSocket.h"
#include "NetSocketListener.h"
#include "NetThread.h"
#include "NetUdpPacket.h"

#include <float.h>

#include "HiResTime.h"
#include "Debug.h"
#include "Preferences.h"
#include "Profiler.h"
#include "Input.h"

#include "ProtocolLimits.h"
#include "Server.h"
#include "ClientToServer.h"
#include "ServerToClientLetter.h"
#include "Generic.h"


// The socket callbacks are plain function pointers and cannot carry state, so
// the running client has to be reachable from file scope. This replaces
// g_app->m_clientToServer; it is set in the constructor.
static ClientToServer* s_client = nullptr;

static NetCallBackRetType ListenCallback(NetUdpPacket *udpdata)
{
    if (udpdata)
    {
        NetIpAddress *fromAddr = &udpdata->m_clientAddress;
        char newip[16];
		IpToString(fromAddr->sin_addr, newip);

        ServerToClientLetter *letter = new ServerToClientLetter(udpdata->m_data, udpdata->m_length);
        s_client->ReceiveLetter(letter);
        //        SET_PROFILE(m_profiler,  "#Client Receive", udpdata->getLength() );

        delete udpdata;
    }

    return 0;

}


static NetCallBackRetType ListenThread(void *ignored)
{
  s_client->m_receiveSocket = new NetSocketListener(4001);
  NetRetCode retCode = s_client->m_receiveSocket->StartListening(ListenCallback);
  DEBUG_ASSERT(retCode == NetOk);
  return 0;
}


ClientToServer::ClientToServer()
{
  s_client = this;

  m_lastValidSequenceIdFromServer = -1;
  m_startTime = DBL_MAX; // Same initial value g_startTime carried in Main.cpp

  m_inboxMutex = new NetMutex();
  m_outboxMutex = new NetMutex();

  m_netLib = new NetLib();
  m_netLib->Initialise();

  m_sendSocket = new NetSocket();
  char const* serverAddress = g_prefsManager->GetString("ServerAddress");
  m_sendSocket->Connect(serverAddress, 4000);

  // Null it before the listen thread starts: the thread's first act is to store
  // its listener here, and this write used to be able to land on top of that.
  m_receiveSocket = NULL;

  NetStartThread(ListenThread);
}


ClientToServer::~ClientToServer()
{
    while( m_outbox.Size() > 0 ) {}

	m_inbox.EmptyAndDelete();
	m_outbox.EmptyAndDelete();
	SAFE_DELETE(m_inboxMutex);
	SAFE_DELETE(m_outboxMutex);
	SAFE_DELETE(m_netLib);
	SAFE_DELETE(m_sendSocket);
	SAFE_DELETE(m_receiveSocket);
}


void ClientToServer::AdvanceSender()
{
    int bytesSentThisFrame = 0;
    m_outboxMutex->Lock();

    while (m_outbox.Size())
    {
      NetworkUpdate* letter = m_outbox[0];
      DEBUG_ASSERT(letter);

      {
        int letterSize = 0;
        char* byteStream = letter->GetByteStream(&letterSize);
        NetSocket* socket = m_sendSocket;
        socket->WriteData(byteStream, letterSize);
        bytesSentThisFrame += letterSize;
        delete letter;
      }

      m_outbox.RemoveData(0);
    }
    m_outboxMutex->Unlock();

    if( bytesSentThisFrame > 0 )
    {
      //        SET_PROFILE(m_profiler,  "#Client Send", bytesSentThisFrame );
    }
}


void ClientToServer::Advance()
{
	AdvanceSender();
}


int ClientToServer::GetOurIP_Int()
{
	// We're not doing networking for now
	static int s_localIP = ConvertIPToInt( "127.0.0.1" );
	return s_localIP;

// Notes by John
// =============
//
// The commented code below has the following problems
//
// - it doesn't always return the same IP (sometimes 127.0.0.1
//   and sometimes the real ip). This means that the remote packet
//   detection code in ProcessServerLetters in main.cpp
//   can incorrectly classify a TeamAssignment message as Remote
//   when it should be local.
//
// - on many machines the hostname has nothing to do with IP address. I
//   believe the correct thing to do is open a TCP connection to the
//   server and ask the server what it thinks your IP address is
//   (see getpeername). This will work even if the client is behind a NAT.
//
// - h_addr_list[0] is in network byte order. Treating it directly
//   as an int leads to endianness problems. ConvertIntToIP and ConvertIPToInt
//   assume that the least significant byte of the integer
//   corresponds to the A of the ip address A.B.C.D. The problem is that
//   a direct cast from h_addr_list[0] to an integer means that the least
//   significant byte be different on big endian machines (Macintosh). The effect
//   of this is that the IP comes out in reverse order on the Mac.
//		See functions ntohl and hton for possible solutions.
//
// - Constant parsing of IP strings and generation again seems wasteful. I think
//   that IPs should be represented as a class, with various different constructors.
//	 The private data should be 4 unsigned chars. With this strategy you could
//   even support IPv6 (if that actually happens before the year 3000).

//	char hostName[256];
//
//	int errorCode = gethostname( hostName, sizeof(hostName) );
//	if (errorCode == 0) {
//		struct hostent *hostEnt = gethostbyname(hostName);
//		if (hostEnt && hostEnt->h_addr_list[0])
//			return *((int*)hostEnt->h_addr_list[0]);
//	}
//	return ConvertIPToInt( "127.0.0.1" );
}


char *ClientToServer::GetOurIP_String()
{
    static char *result = NULL;

    if( !result )
    {
        result = new char[16];
        strcpy( result, ConvertIntToIP( GetOurIP_Int() ) );
    }

    return result;
}


int ClientToServer::GetNextLetterSeqID()
{
    int result = -1;

    m_inboxMutex->Lock();
    if( m_inbox.Size() > 0 )
    {
        result = m_inbox[0]->GetSequenceId();
    }
    m_inboxMutex->Unlock();

    return result;
}


ServerToClientLetter* ClientToServer::GetNextLetter(int _lastProcessedSequenceId)
{
    m_inboxMutex->Lock();
    ServerToClientLetter *letter = NULL;

    if( m_inbox.Size() > 0 )
    {
        letter = m_inbox[0];
        if (letter->GetSequenceId() == _lastProcessedSequenceId + 1)
        {
            m_inbox.RemoveData(0);
        }
        else
        {
            letter = NULL;
        }
    }

    m_inboxMutex->Unlock();

    return letter;
}


void ClientToServer::ReceiveLetter( ServerToClientLetter *letter )
{
    //
    // Simulate network packet loss

#ifdef _DEBUG
    if( g_inputManager->controlEvent( ControlDebugDropPacket ) )
    {
        delete letter;
        return;
    }
#endif

    //
    // Check for duplicates

    if( letter->GetSequenceId() <= m_lastValidSequenceIdFromServer )
    {
        delete letter;
        return;
    }

    //
    // Work out our start time

    double newStartTime = GetHighResTime() - (float) letter->GetSequenceId() * SERVER_ADVANCE_PERIOD;
    if (newStartTime < m_startTime)
    {
      m_startTime = newStartTime;
#ifdef _DEBUG
      // DebugTrace( "Start Time set to %f\n", (float) m_startTime );
#endif
    }
//#ifdef _DEBUG
    else if (newStartTime > m_startTime + 0.1f)
    {
      m_startTime = newStartTime;
      //        DebugTrace( "Start Time set to %f\n", (float) m_startTime );
    }
//#endif

    //
    // Do a sorted insert of the letter into the inbox

    m_inboxMutex->Lock();
    int i;
    bool inserted = false;
    for( i = m_inbox.Size()-1; i >= 0; --i )
    {
        ServerToClientLetter *thisLetter = m_inbox[i];
        if( letter->GetSequenceId() > thisLetter->GetSequenceId() )
        {
            m_inbox.PutDataAtIndex( letter, i+1 );
            inserted = true;
            break;
        }
        else if( letter->GetSequenceId() == thisLetter->GetSequenceId() )
        {
            // Throw this letter away, it's a duplicate
            delete letter;
            inserted = true;
            break;
        }
    }
    if( !inserted )
    {
        m_inbox.PutDataAtStart( letter );
    }


    //
    // Recalculate our last Known Sequence Id

    for( i = 0; i < m_inbox.Size(); ++i )
    {
        ServerToClientLetter *thisLetter = m_inbox[i];
        if( thisLetter->GetSequenceId() > m_lastValidSequenceIdFromServer+1 )
        {
            break;
        }
        m_lastValidSequenceIdFromServer = thisLetter->GetSequenceId();
    }

    m_inboxMutex->Unlock();
}


void ClientToServer::SendLetter( NetworkUpdate *letter )
{
    letter->SetLastSequenceId( m_lastValidSequenceIdFromServer );

    m_outboxMutex->Lock();
    m_outbox.PutDataAtEnd( letter );
    m_outboxMutex->Unlock();
}


void ClientToServer::ClientJoin()
{
    DebugTrace( "CLIENT : Attempting connection...\n" );
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::ClientJoin );
    SendLetter( letter );
}


void ClientToServer::ClientLeave()
{
    DebugTrace( "CLIENT : Sending disconnect...\n" );
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::ClientLeave );
    SendLetter( letter );

    // Only the endpoint's own counter is reset here. The caller resets the one
    // that tracks how far the simulation has advanced, because that is its.
    m_lastValidSequenceIdFromServer = -1;
}


void ClientToServer::RequestTeam(int _teamType, int _desiredId)
{
    DebugTrace( "CLIENT : Requesting Team...\n" );

    NetworkUpdate *letter = new NetworkUpdate();
	letter->SetDesiredTeamId(_desiredId);
    letter->SetType( NetworkUpdate::RequestTeam );
    letter->SetTeamType( _teamType );
    SendLetter( letter );
}


void ClientToServer::SendIAmAlive( unsigned char _teamId, TeamControls const &_teamControls )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::Alive );
    letter->SetTeamId( _teamId );
    letter->SetWorldPos( _teamControls.m_mousePos );
    letter->SetTeamControls( _teamControls );
    SendLetter( letter );
}


void ClientToServer::SendSyncronisation( int _lastProcessedId, unsigned char _sync )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::Syncronise );
    letter->SetLastProcessedId( _lastProcessedId );
    letter->SetSync( _sync );
    SendLetter( letter );
}


void ClientToServer::RequestPause()
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::Pause );
    SendLetter( letter );
}


void ClientToServer::RequestSelectUnit( unsigned char _teamId, int _unitId, int _entityId, int _buildingId )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::SelectUnit );
    letter->SetTeamId( _teamId );
    letter->SetUnitId( _unitId );
    letter->SetEntityId( _entityId );
    letter->SetBuildingID( _buildingId );
    SendLetter( letter );
}


void ClientToServer::RequestCreateUnit( unsigned char _teamId, unsigned char _troopType, int _numToCreate, int _buildingId )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::CreateUnit );
    letter->SetTeamId( _teamId );
    letter->SetEntityType( _troopType );
    letter->SetNumTroops( _numToCreate );
    letter->SetBuildingID( _buildingId );
    SendLetter( letter );
}


void ClientToServer::RequestCreateUnit( unsigned char _teamId, unsigned char _troopType, int _numToCreate, Vector3 const &_pos )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::CreateUnit );
    letter->SetTeamId( _teamId );
    letter->SetEntityType( _troopType );
    letter->SetNumTroops( _numToCreate );
    letter->SetBuildingID( -1 );
    letter->SetWorldPos( _pos );
    SendLetter( letter );
}


void ClientToServer::RequestAimBuilding( unsigned char _teamId, int _buildingId, Vector3 const &_pos )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::AimBuilding );
    letter->SetTeamId( _teamId );
    letter->SetBuildingID( _buildingId );
    letter->SetWorldPos( _pos );
    SendLetter( letter );
}


void ClientToServer::RequestToggleFence( int _buildingId )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::ToggleLaserFence );
    letter->SetBuildingID( _buildingId );
    SendLetter( letter );
}


void ClientToServer::RequestRunProgram( unsigned char _teamId, unsigned char _program )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::RunProgram );
    letter->SetTeamId( _teamId );
    letter->SetProgram( _program );
    SendLetter( letter );
}


void ClientToServer::RequestTargetProgram( unsigned char _teamId, unsigned char _program, Vector3 const &_pos )
{
    NetworkUpdate *letter = new NetworkUpdate();
    letter->SetType( NetworkUpdate::TargetProgram );
    letter->SetTeamId( _teamId );
    letter->SetProgram( _program );
    letter->SetWorldPos( _pos );
    SendLetter( letter );
}
