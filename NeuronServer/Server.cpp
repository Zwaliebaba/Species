#include "pch.h"

#include "NetLib.h"
#include "Transport.h"

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
    // The file-scope Server* that used to live here went with the listen
    // thread. It existed because a socket callback is a plain function pointer
    // and cannot carry state; nothing takes a callback now.
    auto transport = std::make_unique<UdpTransport>();

    const std::error_code failure = transport->Open(ServerPort);
    if (failure)
      NetDebugOut("Server could not open port {}: {}", ServerPort, failure.message());

    Initialise(_profiler, std::move(transport));
  }

  void Server::Initialise(Profiler* _profiler, std::unique_ptr<Transport> _transport)
  {
    m_profiler = _profiler;
    m_transport = std::move(_transport);

    // Winsock is started even when the transport is a fake one. It is a
    // process-wide reference count rather than anything this Server holds, and
    // a test that skipped it would be running a different Initialise from the
    // game's.
    m_netLib = new NetLib();
    m_netLib->Initialise();
  }

  // *** ReceiveDatagrams
  void Server::ReceiveDatagrams()
  {
    if (!m_transport)
      return;

    char datagram[MaxDatagramSize];
    int received = 0;
    Endpoint from;

    while (m_transport->TryReceive(datagram, static_cast<int>(sizeof(datagram)), received, from))
    {
      auto update = std::make_unique<NetworkUpdate>(datagram, received);
      if (!update->IsValid())
        continue;

      ReceiveLetter(std::move(update), from);
    }
  }

  int Server::GetClientId(Endpoint const& _from) const
  {
    for (int i = 0; i < m_clients.Size(); ++i)
    {
      if (m_clients.ValidIndex(i) && m_clients[i]->GetEndpoint() == _from)
        return i;
    }

    return -1;
  }


  // ***RegisterNewClient
  void Server::RegisterNewClient(Endpoint const& _from, int _joinToken)
  {
    DEBUG_ASSERT(GetClientId(_from) == -1);

    // The slot index IS the connection id. It is already what the outbox routes
    // by, it is stable for the life of the connection, and SlotMap's contract is
    // that an entry's index never changes — see CODING_STANDARDS.md, which says
    // the same thing about the indices that are network identity elsewhere.
    const int connectionId = m_clients.GetNextFree();
    m_clients[connectionId] = std::make_unique<ServerToClient>(_from, connectionId);

    //
    // Tell all clients about it. Every client receives this, because the
    // sequence stream has to be identical for all of them — the joining client
    // picks its own out by the token it chose and the server echoes.

    auto letter = std::make_unique<ServerToClientLetter>();
    letter->SetType(ServerToClientLetter::LetterType::HelloClient);
    letter->SetConnectionId(connectionId);
    letter->SetJoinToken(_joinToken);
    SendLetter(std::move(letter));
  }

  void Server::RemoveClient(Endpoint const& _from)
  {
    int clientId = GetClientId(_from);
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
    letter->SetConnectionId(clientId);
    SendLetter(std::move(letter));
  }

  // *** RegisterNewTeam
  void Server::RegisterNewTeam(Endpoint const& _from, int _teamType, int _desiredTeamId)
  {
    int clientId = GetClientId(_from);
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
      if (m_teams.NumUsed() >= NUM_TEAMS)
      {
        // A refusal, not an assert. There are NUM_TEAMS teams and a fifth
        // player asking for one is an ordinary thing to happen on a full
        // server — it used to be a Debug crash and, in Release, a team written
        // past the end of the table.
        NetDebugOut("SERVER: refusing a team request, all {} teams are taken", NUM_TEAMS);
        return;
      }

      int teamId = m_teams.PutData(std::make_unique<ServerTeam>(clientId));

      auto letter = std::make_unique<ServerToClientLetter>();
      letter->SetType(ServerToClientLetter::LetterType::TeamAssign);
      letter->SetTeamId(teamId);
      letter->SetConnectionId(clientId);
      letter->SetTeamType(_teamType);
      SendLetter(std::move(letter));
    }

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

  void Server::ReceiveLetter(std::unique_ptr<NetworkUpdate> update, Endpoint const& _from)
  {
    // Stamped from the datagram's source rather than taken from anything the
    // sender wrote. A client cannot claim to be another client by saying so.
    update->m_source = _from;
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
    if (!m_transport)
      return;

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
          const std::error_code failure = m_transport->Send(client->GetEndpoint(), datagram, linearSize);
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

  // *** PruneHistory
  void Server::PruneHistory()
  {
    // Everything at or below the lowest sequence id every live client has
    // acknowledged can never be asked for again, so it is dropped. Without
    // this the history is every letter the server has ever sent: a host that
    // runs for an hour at 10 Hz holds 36,000 of them, and one that runs for a
    // day cannot.
    //
    // With no clients there is nobody to retransmit to and the whole window
    // goes. That is also what makes a crashed client's history recoverable —
    // it is the timeout above that eventually removes the client holding the
    // window open.
    int lowestAcknowledged = m_sequenceId - 1;

    for (int i = 0; i < m_clients.Size(); ++i)
    {
      if (m_clients.ValidIndex(i) && m_clients[i]->m_lastKnownSequenceId < lowestAcknowledged)
        lowestAcknowledged = m_clients[i]->m_lastKnownSequenceId;
    }

    const int dropUpTo = lowestAcknowledged + 1 - m_historyBaseSequenceId;
    if (dropUpTo <= 0)
      return;

    const int available = static_cast<int>(m_history.size());
    const int drop = dropUpTo < available ? dropUpTo : available;

    m_history.erase(m_history.begin(), m_history.begin() + static_cast<std::ptrdiff_t>(drop));
    m_historyBaseSequenceId += drop;
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

    // A desync is acted on after the drain rather than inside it: removing a
    // client mid-loop would invalidate the id the rest of this iteration uses.
    bool clientDesynced = false;
    Endpoint desyncedClient;

    std::unique_ptr<NetworkUpdate> incoming = GetNextLetter();

    while (incoming)
    {
      if (incoming->m_type == NetworkUpdate::UpdateType::ClientJoin)
      {
        if (GetClientId(incoming->m_source) == -1)
        {
          DebugTrace("SERVER: New Client connected from {}\n", incoming->m_source.ToString());
          RegisterNewClient(incoming->m_source, incoming->m_joinToken);
        }
      }
      else if (incoming->m_type == NetworkUpdate::UpdateType::ClientLeave)
      {
        if (GetClientId(incoming->m_source) != -1)
        {
          DebugTrace("SERVER: Client at {} disconnected gracefully\n", incoming->m_source.ToString());
          RemoveClient(incoming->m_source);
        }
      }
      else if (incoming->m_type == NetworkUpdate::UpdateType::RequestTeam)
      {
        if (GetClientId(incoming->m_source) != -1)
        {
          DebugTrace("SERVER: New team request from {}\n", incoming->m_source.ToString());
          RegisterNewTeam(incoming->m_source, incoming->m_teamType, incoming->m_desiredTeamId);
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
            if (lastKnownSync != sync)
            {
              // A DESYNC, AT RUNTIME. This was a DEBUG_ASSERT, so a Release
              // build played on with two clients simulating different worlds and
              // nothing said so — which is the worst outcome available, because
              // the divergence compounds silently. The offending client is told
              // to go rather than left in a session it no longer agrees with.
              NetDebugOut("SERVER: desync at sequence {} — client {} says {}, everyone else says {}", sequenceId, GetClientId(incoming->m_source),
                          static_cast<int>(sync), static_cast<int>(lastKnownSync));
              desyncedClient = incoming->m_source;
              clientDesynced = true;
            }
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

      int clientId = GetClientId(incoming->m_source);
      if (clientId != -1)
      {
        ServerToClient* sToc = m_clients[clientId].get();
        sToc->m_ticksSinceHeardFrom = 0;
        if (incoming->m_lastSequenceId > sToc->m_lastKnownSequenceId)
          sToc->m_lastKnownSequenceId = incoming->m_lastSequenceId;
      }

      incoming = GetNextLetter();
    }

    if (clientDesynced && GetClientId(desyncedClient) != -1)
      RemoveClient(desyncedClient);

    //
    // Anything that has not been heard from for long enough is gone. There was
    // no timeout at all before this: RemoveClient fired only on a graceful
    // ClientLeave, so a client that crashed was retransmitted to forever and
    // its unacknowledged history could never be pruned.

    for (int i = 0; i < m_clients.Size(); ++i)
    {
      if (!m_clients.ValidIndex(i))
        continue;

      ++m_clients[i]->m_ticksSinceHeardFrom;
      if (m_clients[i]->m_ticksSinceHeardFrom >= ClientTimeoutTicks)
      {
        NetDebugOut("SERVER: client {} silent for {} ticks, disconnecting", i, m_clients[i]->m_ticksSinceHeardFrom);

        // A copy, not GetEndpoint()'s reference: RemoveClient destroys the
        // ServerToClient that reference points into.
        const Endpoint endpoint = m_clients[i]->GetEndpoint();
        RemoveClient(endpoint);
      }
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
        int sendTo = m_historyBaseSequenceId + static_cast<int>(m_history.size());
        if (sendTo - sendFrom > maxUpdates)
          sendTo = sendFrom + maxUpdates;

        for (int l = sendFrom; l < sendTo; ++l)
        {
          // l is a SEQUENCE ID and m_history is a window, so the two are only
          // the same while nothing has been pruned. They were the same thing
          // before pruning existed, which is why this used to index directly.
          const int index = l - m_historyBaseSequenceId;
          if (index >= 0 && index < static_cast<int>(m_history.size()))
          {
            ServerToClientLetter* theLetter = m_history[index].get();
            auto letterCopy = std::make_unique<ServerToClientLetter>(*theLetter);
            letterCopy->SetClientId(i);

            m_outbox.push_back(std::move(letterCopy));
          }
        }
      }
    }

    PruneHistory();
    AdvanceSender();

    END_PROFILE(m_profiler, "Advance Server");
  }
} // namespace Neuron
