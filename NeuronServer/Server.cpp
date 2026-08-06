#include "pch.h"

#include "NetLib.h"
#include "UdpSocket.h"

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

  Server::Server()
    : m_netLib(nullptr),
      m_profiler(nullptr),
      m_sequenceId(0)
  {
    m_sync.SetSize(0);
  }

  Server::~Server()
  {
    m_history.clear();
    m_clients.Empty();
    m_teams.Empty();
    m_inbox.clear();
    m_outbox.clear();

    // No mutexes and no thread to stop. The listen thread this used to leave
    // running — still holding a pointer to a Server about to be destroyed, and
    // safe only because the process was about to exit — does not exist any
    // more; the socket closes with the member.
  }

  void Server::Initialise(Profiler* _profiler)
  {
    m_profiler = _profiler;

    m_netLib = new NetLib();
    m_netLib->Initialise();

    // The file-scope Server* that used to live here went with the listen
    // thread. It existed because a socket callback is a plain function pointer
    // and cannot carry state; nothing takes a callback now.
    const std::error_code failure = m_socket.Open(ServerPort);
    if (failure)
      NetDebugOut("Server could not open port {}: {}", ServerPort, failure.message());
  }

  // *** ReceiveDatagrams
  void Server::ReceiveDatagrams()
  {
    if (!m_socket.IsOpen())
      return;

    char datagram[MaxDatagramSize];
    int received = 0;
    Endpoint from;

    while (m_socket.TryReceive(datagram, static_cast<int>(sizeof(datagram)), received, from))
    {
      auto update = std::make_unique<NetworkUpdate>(datagram, received);
      if (!update->IsValid())
        continue;

      in_addr address;
      address.s_addr = from.m_address;
      ReceiveLetter(std::move(update), IpToString(address));
    }
  }

  int Server::GetClientId(char* _ip)
  {
    for (int i = 0; i < m_clients.Size(); ++i)
    {
      if (m_clients.ValidIndex(i))
      {
        std::string_view const thisIP = m_clients[i]->GetIP();
        if (thisIP == _ip)
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
    letter->SetType(ServerToClientLetter::LetterType::HelloClient);
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
    letter->SetType(ServerToClientLetter::LetterType::GoodbyeClient);
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
      letter->SetType(ServerToClientLetter::LetterType::TeamAssign);
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
                letter->SetType( ServerToClientLetter::LetterType::TeamAssign );
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
    std::unique_ptr<NetworkUpdate> letter;

    if (!m_inbox.empty())
    {
      letter = std::move(m_inbox.front());
      m_inbox.erase(m_inbox.begin());
    }

    return letter;
  }

  void Server::ReceiveLetter(std::unique_ptr<NetworkUpdate> update, std::string_view fromIP)
  {
    update->SetClientIp(fromIP);
    m_inbox.push_back(std::move(update));
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
        // The buffer is this loop's, not the letter's: serialising used to
        // write into one file-scope array shared by every letter in the
        // process.
        char datagram[MaxDatagramSize];
        const int linearSize = letter->Serialise(datagram, static_cast<int>(sizeof(datagram)));
        if (linearSize > 0)
        {
          // Every client is written to through the server's one socket, rather
          // than through a NetSocket per client that had to be "connected"
          // before it could be used. Identity does not change here — the reply
          // still goes to the address the client registered with, on the fixed
          // client port. T9 is what makes it the address the datagram actually
          // arrived from.
          ServerToClient* client = m_clients[letter->GetClientId()].get();
          const std::error_code failure = m_socket.SendTo(client->GetEndpoint(), datagram, linearSize);
          if (failure)
            NetDebugOut("Server send to {} failed: {}", client->GetEndpoint().ToString(), failure.message());
          else
            bytesSentThisFrame += linearSize;
        }
      }
    }

    if (bytesSentThisFrame > 0)
    {
      //        SET_PROFILE(m_profiler,  "#Server Send", (double) bytesSentThisFrame );
    }
  }

  void FillLetter(ServerToClientLetter& _letter, std::vector<NetworkUpdate>& _pending)
  {
    size_t taken = 0;
    while (taken < _pending.size() && _letter.AddUpdate(_pending[taken]))
      ++taken;

    // The first refusal ends it. Skipping the one that did not fit and
    // continuing with the next — which is what "add everything that fits"
    // would mean — would reorder what the clients apply, and every client
    // advances its own copy of the world from that order.
    _pending.erase(_pending.begin(), _pending.begin() + static_cast<std::ptrdiff_t>(taken));
  }

  void Server::Advance()
  {
    START_PROFILE(m_profiler, "Advance Server");

    //
    // Take everything the socket has been holding since the last tick. This
    // used to arrive on a listen thread as it happened, and sit in the inbox
    // behind a mutex until exactly this point — so draining here rather than
    // there costs nothing and removes the only reason the server had a second
    // thread.

    ReceiveDatagrams();

    //
    // Compile all incoming messages into a ServerToClientLetter

    auto letter = std::make_unique<ServerToClientLetter>();
    letter->SetType(ServerToClientLetter::LetterType::Update);

    std::unique_ptr<NetworkUpdate> incoming = GetNextLetter();

    while (incoming)
    {
      if (incoming->m_type == NetworkUpdate::UpdateType::ClientJoin)
      {
        if (GetClientId(incoming->m_clientIp) == -1)
        {
          DebugTrace("SERVER: New Client connected from {}\n", incoming->m_clientIp);
          RegisterNewClient(incoming->m_clientIp);
        }
      }
      else if (incoming->m_type == NetworkUpdate::UpdateType::ClientLeave)
      {
        if (GetClientId(incoming->m_clientIp) != -1)
        {
          DebugTrace("SERVER: Client at {} disconnected gracefully\n", incoming->m_clientIp);
          RemoveClient(incoming->m_clientIp);
        }
      }
      else if (incoming->m_type == NetworkUpdate::UpdateType::RequestTeam)
      {
        if (GetClientId(incoming->m_clientIp) != -1)
        {
          DebugTrace("SERVER: New team request from {}\n", incoming->m_clientIp);
          RegisterNewTeam(incoming->m_clientIp, incoming->m_teamType, incoming->m_desiredTeamId);
        }
      }
      else if (incoming->m_type == NetworkUpdate::UpdateType::Syncronise)
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
        // Queued rather than added to the letter here. Whether it fits is a
        // question about the whole tick, and answering it one update at a time
        // inside this loop is what made an over-full letter possible: the
        // updates that did not fit have to go to the NEXT letter, in order,
        // which cannot be decided until the loop has finished.
        m_pendingUpdates.push_back(*incoming);

      int clientId = GetClientId(incoming->m_clientIp);
      if (clientId != -1)
      {
        ServerToClient* sToc = m_clients[clientId].get();
        if (incoming->m_lastSequenceId > sToc->m_lastKnownSequenceId)
          sToc->m_lastKnownSequenceId = incoming->m_lastSequenceId;
      }

      incoming = GetNextLetter();
    }

    FillLetter(*letter, m_pendingUpdates);
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

            m_outbox.push_back(std::move(letterCopy));
          }
        }
      }
    }

    AdvanceSender();

    END_PROFILE(m_profiler, "Advance Server");
  }
} // namespace Neuron
