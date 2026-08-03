#include "pch.h"

#include "NetLib.h"
#include "NetMutex.h"
#include "NetSocket.h"
#include "NetSocketListener.h"
#include "NetThread.h"
#include "NetUdpPacket.h"

#include "Debug.h"
#include "Profiler.h"
#include "Preferences.h"


#include "ProtocolLimits.h"
#include "Generic.h"
#include "Server.h"
#include "ServerToClient.h"
#include "ServerToClientLetter.h"

// ****************************************************************************
// Class ServerTeam
// ****************************************************************************

ServerTeam::ServerTeam(int _clientId)
  : m_clientId(_clientId) {}

// ****************************************************************************
// Class Server
// ****************************************************************************

// The socket listener takes a plain function pointer, so the running server has
// to be reachable from file scope. This replaces g_app->m_server; it is set in
// Initialise and is the only global state left in this file.
static Server* s_server = nullptr;

// ***ListenCallback
static NetCallBackRetType ListenCallback(NetUdpPacket* udpdata)
{
  if (udpdata)
  {
    NetIpAddress* fromAddr = &udpdata->m_clientAddress;
    char newip[16];
    IpToString(fromAddr->sin_addr, newip);

    if (s_server)
    {
      auto letter = new NetworkUpdate(udpdata->m_data);
      s_server->ReceiveLetter(letter, newip);
      //            SET_PROFILE(m_profiler,  "#Server Receive", (double) udpdata->getLength() );
    }

    delete udpdata;
  }

  return 0;
}

Server::Server()
  : m_netLib(nullptr),
    m_profiler(nullptr),
    m_sequenceId(0),
    m_inboxMutex(nullptr),
    m_outboxMutex(nullptr)
{
  m_sync.SetSize(0);
}

Server::~Server()
{
  for (auto* letter : m_history)
  {
    delete letter;
  }
  m_history.clear();
  m_clients.EmptyAndDelete();
  m_teams.EmptyAndDelete();

  // The mutexes are created by Initialise, not by the constructor, so a Server
  // that was built and never initialised used to null-dereference here. Species
  // always pairs the two, which is why nothing hit it — but a class you cannot
  // destroy without starting its network threads is a class no test can hold.
  if (m_inboxMutex)
  {
    m_inboxMutex->Lock();
    for (auto* update : m_inbox)
    {
      delete update;
    }
    m_inbox.clear();
    m_inboxMutex->Unlock();
  }

  if (m_outboxMutex)
  {
    m_outboxMutex->Lock();
    for (auto* letter : m_outbox)
    {
      delete letter;
    }
    m_outbox.clear();
    m_outboxMutex->Unlock();
  }
}

static NetCallBackRetType ListenThread(void* ptr)
{
  auto m_listener = new NetSocketListener(4000);
  m_listener->StartListening(ListenCallback);
  return 0;
}

void Server::Initialise(Profiler* _profiler)
{
  m_profiler = _profiler;
  s_server = this;

  m_inboxMutex = new NetMutex();
  m_outboxMutex = new NetMutex();

  m_netLib = new NetLib();
  m_netLib->Initialise();

  NetStartThread(ListenThread);
}

int Server::GetClientId(char* _ip)
{
  for (int i = 0; i < m_clients.Size(); ++i)
  {
    if (m_clients.ValidIndex(i))
    {
      char* thisIP = m_clients[i]->GetIP();
      if (strcmp(thisIP, _ip) == 0)
        return i;
    }
  }

  return -1;
}



// ***RegisterNewClient
void Server::RegisterNewClient(char* _ip)
{
  DEBUG_ASSERT(GetClientId(_ip) == -1);
  auto sToC = new ServerToClient(_ip);
  m_clients.PutData(sToC);

  //
  // Tell all clients about it

  auto letter = new ServerToClientLetter();
  letter->SetType(ServerToClientLetter::HelloClient);
  letter->SetIp(ConvertIPToInt(_ip));
  SendLetter(letter);
}

void Server::RemoveClient(char* _ip)
{
  int clientId = GetClientId(_ip);
  ServerToClient* sToC = m_clients[clientId];
  m_clients.MarkNotUsed(clientId);
  delete sToC;

  //
  // Tell all clients about it

  auto letter = new ServerToClientLetter();
  letter->SetType(ServerToClientLetter::GoodbyeClient);
  letter->SetIp(ConvertIPToInt(_ip));
  SendLetter(letter);
}

// *** RegisterNewTeam
void Server::RegisterNewTeam(char* _ip, int _teamType, int _desiredTeamId)
{
  int clientId = GetClientId(_ip);
  DEBUG_ASSERT(clientId != -1); // Client not properly connected

  if (_desiredTeamId != -1) // Specified Team ID - An AI
  {
    if (!m_teams.ValidIndex(_desiredTeamId))
    {
      auto team = new ServerTeam(clientId);
      if (m_teams.Size() <= _desiredTeamId)
        m_teams.SetSize(_desiredTeamId + 1);
      m_teams.PutData(team, _desiredTeamId);
    }
  }
  else
  {
    DEBUG_ASSERT(m_teams.NumUsed() < NUM_TEAMS);
    auto team = new ServerTeam(clientId);
    int teamId = m_teams.PutData(team);

    auto letter = new ServerToClientLetter();
    letter->SetType(ServerToClientLetter::TeamAssign);
    letter->SetTeamId(teamId);
    letter->SetIp(ConvertIPToInt(_ip));
    letter->SetTeamType(_teamType);
    SendLetter(letter);
  }

  /*
      DEBUG_ASSERT(GetClientId(_ip) != -1);
  
      if (m_teams.NumUsed() < NUM_TEAMS)
      {
          int clientId = GetClientId(_ip);
          ServerTeam *team = new ServerTeam(clientId);
          int teamId = -1;
  
          if (_desiredTeamId == -1)
      {
        teamId = m_teams.PutData( team );
      }
      else
      {
        DEBUG_ASSERT(!m_teams.ValidIndex(_desiredTeamId));
        teamId = _desiredTeamId;
        if (m_teams.Size() <= _desiredTeamId)
        {
          m_teams.SetSize(_desiredTeamId+1);
        }
        m_teams.PutData(team, _desiredTeamId);
      }
  
          //
          // Send a TeamAssign letter to all Clients
  
          if( _teamType != Team::TeamTypeAI )
          {
              ServerToClientLetter *letter = new ServerToClientLetter();
              letter->SetType( ServerToClientLetter::TeamAssign );
              letter->SetTeamId(teamId);
              letter->SetIp( ConvertIPToInt( _ip ) );
              letter->SetTeamType( _teamType );
              letter->SetAIType( _aiType );
  
              SendLetter( letter );
          }
      }*/
}

NetworkUpdate* Server::GetNextLetter()
{
  m_inboxMutex->Lock();
  NetworkUpdate* letter = nullptr;

  if (!m_inbox.empty())
  {
    letter = m_inbox.front();
    m_inbox.erase(m_inbox.begin());
  }

  m_inboxMutex->Unlock();
  return letter;
}

void Server::ReceiveLetter(NetworkUpdate* update, char* fromIP)
{
  update->SetClientIp(fromIP);

  m_inboxMutex->Lock();
  m_inbox.push_back(update);
  m_inboxMutex->Unlock();
}

void Server::SendLetter(ServerToClientLetter* letter)
{
  //
  // Assign a sequence id

  letter->SetSequenceId(m_sequenceId);
  m_sequenceId++;

  m_history.push_back(letter);
}

void Server::AdvanceSender()
{
  int bytesSentThisFrame = 0;
  m_outboxMutex->Lock();

  while (!m_outbox.empty())
  {
    ServerToClientLetter* letter = m_outbox.front();
    DEBUG_ASSERT(letter);

    if (m_clients.ValidIndex(letter->GetClientId()))
    {
      {
        int linearSize = 0;
        ServerToClient* client = m_clients[letter->GetClientId()];
        NetSocket* socket = client->GetSocket();
        char* linearisedLetter = letter->GetByteStream(&linearSize);
        socket->WriteData(linearisedLetter, linearSize);
        bytesSentThisFrame += linearSize;
        delete letter;
      }
    }

    // The letter has now been sent so we can take it off the outbox list
    m_outbox.erase(m_outbox.begin());
  }

  m_outboxMutex->Unlock();

  if (bytesSentThisFrame > 0)
  {
    //        SET_PROFILE(m_profiler,  "#Server Send", (double) bytesSentThisFrame );
  }
}

void Server::Advance()
{
  START_PROFILE(m_profiler, "Advance Server");

  //
  // Compile all incoming messages into a ServerToClientLetter

  auto letter = new ServerToClientLetter();
  letter->SetType(ServerToClientLetter::Update);

  NetworkUpdate* incoming = GetNextLetter();

  while (incoming)
  {
    if (incoming->m_type == NetworkUpdate::ClientJoin)
    {
      if (GetClientId(incoming->m_clientIp) == -1)
      {
        DebugTrace("SERVER: New Client connected from %s\n", incoming->m_clientIp);
        RegisterNewClient(incoming->m_clientIp);
      }
    }
    else if (incoming->m_type == NetworkUpdate::ClientLeave)
    {
      if (GetClientId(incoming->m_clientIp) != -1)
      {
        DebugTrace("SERVER: Client at %s disconnected gracefully\n", incoming->m_clientIp);
        RemoveClient(incoming->m_clientIp);
      }
    }
    else if (incoming->m_type == NetworkUpdate::RequestTeam)
    {
      if (GetClientId(incoming->m_clientIp) != -1)
      {
        DebugTrace("SERVER: New team request from %s\n", incoming->m_clientIp);
        RegisterNewTeam(incoming->m_clientIp, incoming->m_teamType, incoming->m_desiredTeamId);
      }
    }
    else if (incoming->m_type == NetworkUpdate::Syncronise)
    {
      int sequenceId = incoming->m_lastProcessedSeqId;
      unsigned char sync = incoming->m_sync;

      if (sequenceId != 0 && !m_sync.ValidIndex(sequenceId - 1))
      {
        // This incoming packet has a sequence ID that is too high
        // Most likely it was sent from a previous client connected to a previous server
        // Then that server shut down, and this one started up
        // Then this server received the packet intended for the old server
        // So we simply discard it
        //DebugTrace( "Sync %d discarded as bogus\n", sequenceId );
      }
      else
      {
        if (m_sync.Size() <= sequenceId)
          m_sync.SetSize(m_sync.Size() + 1000);

        if (m_sync.ValidIndex(sequenceId))
        {
          unsigned char lastKnownSync = m_sync[sequenceId];
          DEBUG_ASSERT(lastKnownSync == sync);
          //DebugTrace( "Sync %02d verified as %03d\n", sequenceId, sync );
        }
        else
        {
          m_sync.PutData(sync, sequenceId);
          //DebugTrace( "Sync %02d set to %03d\n", sequenceId, sync );
        }
      }
    }
    else if (incoming->m_teamId != 255)
      letter->AddUpdate(incoming);

    int clientId = GetClientId(incoming->m_clientIp);
    if (clientId != -1)
    {
      ServerToClient* sToc = m_clients[clientId];
      if (incoming->m_lastSequenceId > sToc->m_lastKnownSequenceId)
        sToc->m_lastKnownSequenceId = incoming->m_lastSequenceId;
    }

    delete incoming;
    incoming = GetNextLetter();
  }

  SendLetter(letter);

  //
  // Update all clients depending on their state

  int maxUpdates = 25; // Sensible to cap re-transmissions like this
  if (g_prefsManager->GetInt("RecordDemo") == 2)
    maxUpdates = 1;

  for (int i = 0; i < m_clients.Size(); ++i)
  {
    if (m_clients.ValidIndex(i))
    {
      ServerToClient* s2c = m_clients[i];
      int sendFrom = s2c->m_lastKnownSequenceId + 1;
      int sendTo = static_cast<int>(m_history.size());
      if (sendTo - sendFrom > maxUpdates)
        sendTo = sendFrom + maxUpdates;

      for (int l = sendFrom; l < sendTo; ++l)
      {
        if (l >= 0 && l < static_cast<int>(m_history.size()))
        {
          ServerToClientLetter* theLetter = m_history[l];
          auto letterCopy = new ServerToClientLetter(*theLetter);
          letterCopy->SetClientId(i);

          m_outboxMutex->Lock();
          m_outbox.push_back(letterCopy);
          m_outboxMutex->Unlock();
        }
      }
    }
  }

  AdvanceSender();

  END_PROFILE(m_profiler, "Advance Server");
}
