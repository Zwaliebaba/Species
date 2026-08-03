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


namespace Neuron
{

  // ****************************************************************************
  // Class ServerTeam
  // ****************************************************************************

  ServerTeam::ServerTeam(int _clientId)
    : m_clientId(_clientId)
  {
  }

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

      if (s_server)
      {
        auto letter = std::make_unique<NetworkUpdate>(udpdata->m_data);
        s_server->ReceiveLetter(std::move(letter), IpToString(fromAddr->sin_addr));
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
    m_history.clear();
    m_clients.Empty();
    m_teams.Empty();

    // The mutexes are created by Initialise, not by the constructor, so a Server
    // that was built and never initialised used to null-dereference here. Species
    // always pairs the two, which is why nothing hit it — but a class you cannot
    // destroy without starting its network threads is a class no test can hold.
    if (m_inboxMutex)
    {
      m_inboxMutex->Lock();
      m_inbox.clear();
      m_inboxMutex->Unlock();
    }

    if (m_outboxMutex)
    {
      m_outboxMutex->Lock();
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
    m_clients.PutData(std::make_unique<ServerToClient>(_ip));

    //
    // Tell all clients about it

    auto letter = std::make_unique<ServerToClientLetter>();
    letter->SetType(ServerToClientLetter::HelloClient);
    letter->SetIp(ConvertIPToInt(_ip));
    SendLetter(std::move(letter));
  }

  void Server::RemoveClient(char* _ip)
  {
    int clientId = GetClientId(_ip);
    // reset() before MarkNotUsed, and both are load-bearing. MarkNotUsed only
    // clears the occupancy bit — it does not destroy — so without the reset the
    // record would outlive the disconnect and linger until the slot was reused.
    // The reset comes first because operator[] requires the slot to still be
    // occupied. The record therefore dies exactly where `delete sToC` used to.
    m_clients[clientId].reset();
    m_clients.MarkNotUsed(clientId);

    //
    // Tell all clients about it

    auto letter = std::make_unique<ServerToClientLetter>();
    letter->SetType(ServerToClientLetter::GoodbyeClient);
    letter->SetIp(ConvertIPToInt(_ip));
    SendLetter(std::move(letter));
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
        if (m_teams.Size() <= _desiredTeamId)
          m_teams.SetSize(_desiredTeamId + 1);
        m_teams.PutData(std::make_unique<ServerTeam>(clientId), _desiredTeamId);
      }
    }
    else
    {
      DEBUG_ASSERT(m_teams.NumUsed() < NUM_TEAMS);
      int teamId = m_teams.PutData(std::make_unique<ServerTeam>(clientId));

      auto letter = std::make_unique<ServerToClientLetter>();
      letter->SetType(ServerToClientLetter::TeamAssign);
      letter->SetTeamId(teamId);
      letter->SetIp(ConvertIPToInt(_ip));
      letter->SetTeamType(_teamType);
      SendLetter(std::move(letter));
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

  std::unique_ptr<NetworkUpdate> Server::GetNextLetter()
  {
    m_inboxMutex->Lock();
    std::unique_ptr<NetworkUpdate> letter;

    if (!m_inbox.empty())
    {
      letter = std::move(m_inbox.front());
      m_inbox.erase(m_inbox.begin());
    }

    m_inboxMutex->Unlock();
    return letter;
  }

  void Server::ReceiveLetter(std::unique_ptr<NetworkUpdate> update, std::string_view fromIP)
  {
    update->SetClientIp(fromIP);

    m_inboxMutex->Lock();
    m_inbox.push_back(std::move(update));
    m_inboxMutex->Unlock();
  }

  void Server::SendLetter(std::unique_ptr<ServerToClientLetter> letter)
  {
    //
    // Assign a sequence id

    letter->SetSequenceId(m_sequenceId);
    m_sequenceId++;

    m_history.push_back(std::move(letter));
  }

  void Server::AdvanceSender()
  {
    int bytesSentThisFrame = 0;
    m_outboxMutex->Lock();

    while (!m_outbox.empty())
    {
      // Taken off the outbox before the send rather than after, so the letter
      // is destroyed when this scope ends whichever branch runs. It used to be
      // deleted only INSIDE the valid-client branch, so a letter addressed to a
      // client that disconnected before it was sent was dropped from the outbox
      // and never freed. Fixing that is a behaviour change and a deliberate one.
      std::unique_ptr<ServerToClientLetter> letter = std::move(m_outbox.front());
      m_outbox.erase(m_outbox.begin());
      DEBUG_ASSERT(letter);

      if (m_clients.ValidIndex(letter->GetClientId()))
      {
        int linearSize = 0;
        ServerToClient* client = m_clients[letter->GetClientId()].get();
        NetSocket* socket = client->GetSocket();
        char* linearisedLetter = letter->GetByteStream(&linearSize);
        socket->WriteData(linearisedLetter, linearSize);
        bytesSentThisFrame += linearSize;
      }
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

    auto letter = std::make_unique<ServerToClientLetter>();
    letter->SetType(ServerToClientLetter::Update);

    std::unique_ptr<NetworkUpdate> incoming = GetNextLetter();

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
          // DebugTrace( "Sync %d discarded as bogus\n", sequenceId );
        }
        else
        {
          if (m_sync.Size() <= sequenceId)
            m_sync.SetSize(m_sync.Size() + 1000);

          if (m_sync.ValidIndex(sequenceId))
          {
            unsigned char lastKnownSync = m_sync[sequenceId];
            DEBUG_ASSERT(lastKnownSync == sync);
            // DebugTrace( "Sync %02d verified as %03d\n", sequenceId, sync );
          }
          else
          {
            m_sync.PutData(sync, sequenceId);
            // DebugTrace( "Sync %02d set to %03d\n", sequenceId, sync );
          }
        }
      }
      else if (incoming->m_teamId != 255)
        // .get(), not a move: AddUpdate copies the struct into the letter's own
        // storage ("Make sure we COPY the update" — new + memcpy), so ownership
        // stays here. The reassignment at the bottom of this loop frees it,
        // which is what `delete incoming` used to do.
        letter->AddUpdate(incoming.get());

      int clientId = GetClientId(incoming->m_clientIp);
      if (clientId != -1)
      {
        ServerToClient* sToc = m_clients[clientId].get();
        if (incoming->m_lastSequenceId > sToc->m_lastKnownSequenceId)
          sToc->m_lastKnownSequenceId = incoming->m_lastSequenceId;
      }

      incoming = GetNextLetter();
    }

    SendLetter(std::move(letter));

    //
    // Update all clients depending on their state

    int maxUpdates = 25; // Sensible to cap re-transmissions like this
    if (g_prefsManager->GetInt("RecordDemo") == 2)
      maxUpdates = 1;

    for (int i = 0; i < m_clients.Size(); ++i)
    {
      if (m_clients.ValidIndex(i))
      {
        ServerToClient* s2c = m_clients[i].get();
        int sendFrom = s2c->m_lastKnownSequenceId + 1;
        int sendTo = static_cast<int>(m_history.size());
        if (sendTo - sendFrom > maxUpdates)
          sendTo = sendFrom + maxUpdates;

        for (int l = sendFrom; l < sendTo; ++l)
        {
          if (l >= 0 && l < static_cast<int>(m_history.size()))
          {
            ServerToClientLetter* theLetter = m_history[l].get();
            auto letterCopy = std::make_unique<ServerToClientLetter>(*theLetter);
            letterCopy->SetClientId(i);

            m_outboxMutex->Lock();
            m_outbox.push_back(std::move(letterCopy));
            m_outboxMutex->Unlock();
          }
        }
      }
    }

    AdvanceSender();

    END_PROFILE(m_profiler, "Advance Server");
  }
} // namespace Neuron
