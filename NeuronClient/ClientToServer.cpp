#include "pch.h"

#include "NetLib.h"
#include "Transport.h"

#include <float.h>

#include "HiResTime.h"
#include "Debug.h"
#include "Preferences.h"
#include "Profiler.h"
#include "Input.h"

// Server.h was included here and never used — no Server appears in this file.
// Left alone it would become a NeuronClient -> NeuronServer include the moment
// T9 moves the host, which is the one direction the layering forbids outright.
// Everything it carried transitively (LList, DArray) either comes from
// ClientToServer.h or is unused here.
#include "ProtocolLimits.h"
#include "ClientToServer.h"
#include "ServerToClientLetter.h"
#include "Generic.h"


namespace Neuron
{
  ClientToServer* g_clientToServer = nullptr;


  namespace
  {
    // Client-local and never simulation state, so the cosmetic generator is the
    // right one — syncrand is the lockstep stream and drawing from it here would
    // shift it by a client-dependent amount. Two draws because one is not
    // guaranteed to be wider than 15 bits.
    int MakeJoinToken() { return (speciesRandom() << 16) ^ speciesRandom(); }
  } // namespace


  ClientToServer::ClientToServer()
  {
    m_connectionId = -1;
    m_joinToken = MakeJoinToken();
    m_lastHeardFromServer = GetHighResTime();
    m_lastValidSequenceIdFromServer = -1;
    m_startTime = DBL_MAX; // Same initial value g_startTime carried in Main.cpp

    m_netLib = std::make_unique<NetLib>();
    m_netLib->Initialise();

    char const* serverAddress = g_prefsManager->GetString("ServerAddress");
    const std::error_code unresolved = ResolveEndpoint(serverAddress, ServerPort, m_serverEndpoint);
    if (unresolved)
      NetDebugOut("Client could not resolve server address {}: {}", serverAddress, unresolved.message());

    // PORT 0: whatever the OS gives us. The client used to bind a fixed 4001 so
    // the server had somewhere to reply to, and that single fact is what
    // limited a host to one client and broke NAT — the server replies to the
    // address a datagram came FROM now, so nothing needs the port to be
    // predictable.
    auto transport = std::make_unique<UdpTransport>();
    const std::error_code failure = transport->Open(0);
    if (failure)
      NetDebugOut("Client could not open a socket: {}", failure.message());

    m_transport = std::move(transport);
  }


  ClientToServer::ClientToServer(std::unique_ptr<Transport> _transport, Endpoint const& _serverEndpoint)
    : m_serverEndpoint(_serverEndpoint),
      m_transport(std::move(_transport))
  {
    // No NetLib and no preferences: this constructor exists so a test can hold
    // a client, and a test has neither. Nothing in the game calls it.
    m_connectionId = -1;
    m_joinToken = MakeJoinToken();
    m_lastHeardFromServer = GetHighResTime();
    m_lastValidSequenceIdFromServer = -1;
    m_startTime = DBL_MAX;
  }


  double ClientToServer::TimeSinceServerHeard() const { return GetHighResTime() - m_lastHeardFromServer; }

  bool ClientToServer::IsServerSilent() const { return TimeSinceServerHeard() > ServerSilenceTimeout; }


  ClientToServer::~ClientToServer()
  {
    // Sends what is queued and closes. What stood here was
    // `while (!m_outbox.empty()) {}` — a spin-wait on a member the listen
    // thread was expected to drain, with no synchronisation and nothing
    // guaranteeing it ever would, and then a reset() of the listener object
    // while that thread was still executing inside it. With the sending on this
    // thread, waiting for it is just doing it.
    AdvanceSender();

    m_inbox.clear();
    m_outbox.clear();
    m_transport.reset();
    m_netLib.reset();
  }


  void ClientToServer::ReceiveDatagrams()
  {
    if (!m_transport)
      return;

    char datagram[MaxDatagramSize];
    int received = 0;
    Endpoint from;

    while (m_transport->TryReceive(datagram, static_cast<int>(sizeof(datagram)), received, from))
    {
      auto letter = std::make_unique<ServerToClientLetter>(datagram, received);

      // Dropped here rather than in the inbox, because ReceiveLetter acts on a
      // letter's sequence id before anything reads its type — it moves the
      // client's clock and its last-valid-sequence id. A letter that did not
      // parse has no sequence id worth believing.
      if (letter->IsValid())
        ReceiveLetter(std::move(letter));
    }
  }


  void ClientToServer::AdvanceSender()
  {
    if (!m_transport)
      return;

    int bytesSentThisFrame = 0;

    while (!m_outbox.empty())
    {
      std::unique_ptr<NetworkUpdate> letter = std::move(m_outbox[0]);
      DEBUG_ASSERT(letter);

      {
        // The whole datagram, frame included. GetByteStream would give the
        // payload alone, which is what a letter carries nested and NOT what a
        // client sends.
        char datagram[MaxDatagramSize];
        const int letterSize = letter->SerialiseDatagram(datagram, static_cast<int>(sizeof(datagram)));
        const std::error_code failure = letterSize > 0 ? m_transport->Send(m_serverEndpoint, datagram, letterSize) : std::error_code();
        if (!failure)
          bytesSentThisFrame += letterSize;
        // reset() rather than letting the scope end it: the old code deleted
        // here, before the erase, and this plan's rule is that ownership moves
        // without the moment of destruction moving with it.
        letter.reset();
      }

      m_outbox.erase(m_outbox.begin());
    }

    if (bytesSentThisFrame > 0)
    {
      //        SET_PROFILE(m_profiler,  "#Client Send", bytesSentThisFrame );
    }
  }


  // Receive before send, so an order given this frame goes out carrying the
  // sequence id of a letter that arrived this frame rather than last frame's.
  void ClientToServer::Advance()
  {
    ReceiveDatagrams();
    AdvanceSender();
  }


  std::unique_ptr<ServerToClientLetter> ClientToServer::GetNextLetter(int _lastProcessedSequenceId)
  {
    std::unique_ptr<ServerToClientLetter> letter;

    if (!m_inbox.empty())
    {
      if (m_inbox[0]->GetSequenceId() == _lastProcessedSequenceId + 1)
      {
        letter = std::move(m_inbox[0]);
        m_inbox.erase(m_inbox.begin());
      }
    }

    return letter;
  }


  void ClientToServer::ReceiveLetter(std::unique_ptr<ServerToClientLetter> letter)
  {
    //
    // Simulate network packet loss. This used to run on the listen thread,
    // which meant a debug build asked the input manager for a control event
    // from a thread that never touches input; it is the main thread now, like
    // everything else here.

#ifdef _DEBUG
    if (g_inputManager->controlEvent(ControlType::ControlDebugDropPacket))
    {
      return;
    }
#endif

    //
    // The server is alive. After the drop hook rather than before it, so a
    // packet the debug build is pretending to lose does not count as having
    // been heard.

    m_lastHeardFromServer = GetHighResTime();

    //
    // Our own welcome, recognised by the token we chose and the server echoed.
    // Done on arrival rather than when the letter is released from the inbox in
    // sequence: this is client bookkeeping, not simulation state, and the game
    // needs the id before it processes the TeamAssign that follows.

    if (letter->m_type == ServerToClientLetter::LetterType::HelloClient && m_connectionId == -1 && letter->m_joinToken == m_joinToken)
    {
      m_connectionId = letter->m_connectionId;
      DebugTrace("CLIENT : the server assigned us connection id {}\n", m_connectionId);
    }

  //
  // Check for duplicates

  if (letter->GetSequenceId() <= m_lastValidSequenceIdFromServer)
  {
    return;
  }

  //
  // Work out our start time

  double newStartTime = GetHighResTime() - (float)letter->GetSequenceId() * SERVER_ADVANCE_PERIOD;
  if (newStartTime < m_startTime)
  {
    m_startTime = newStartTime;
#ifdef _DEBUG
    // DebugTrace( "Start Time set to %f\n", (float) m_startTime );
#endif
  }
  // #ifdef _DEBUG
  else if (newStartTime > m_startTime + 0.1f)
  {
    m_startTime = newStartTime;
    //        DebugTrace( "Start Time set to %f\n", (float) m_startTime );
  }
  // #endif

  //
  // Do a sorted insert of the letter into the inbox

  int i;
  bool inserted = false;
  for (i = static_cast<int>(m_inbox.size()) - 1; i >= 0; --i)
  {
    ServerToClientLetter* thisLetter = m_inbox[i].get();
    if (letter->GetSequenceId() > thisLetter->GetSequenceId())
    {
      m_inbox.insert(m_inbox.begin() + (i + 1), std::move(letter));
      inserted = true;
      break;
    }
    else if (letter->GetSequenceId() == thisLetter->GetSequenceId())
    {
      // Throw this letter away, it's a duplicate
      letter.reset();
      inserted = true;
      break;
    }
  }
  if (!inserted)
  {
    m_inbox.insert(m_inbox.begin(), std::move(letter));
  }


  //
  // Recalculate our last Known Sequence Id

  for (i = 0; i < static_cast<int>(m_inbox.size()); ++i)
  {
    ServerToClientLetter* thisLetter = m_inbox[i].get();
    if (thisLetter->GetSequenceId() > m_lastValidSequenceIdFromServer + 1)
    {
      break;
    }
    m_lastValidSequenceIdFromServer = thisLetter->GetSequenceId();
  }
  }


void ClientToServer::SendLetter(std::unique_ptr<NetworkUpdate> letter)
{
  letter->SetLastSequenceId(m_lastValidSequenceIdFromServer);
  m_outbox.push_back(std::move(letter));
}


void ClientToServer::ClientJoin()
{
  DebugTrace("CLIENT : Attempting connection...\n");
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::ClientJoin);
  SendLetter(std::move(letter));
}


void ClientToServer::ClientLeave()
{
  DebugTrace("CLIENT : Sending disconnect...\n");
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::ClientLeave);
  SendLetter(std::move(letter));

  // Only the endpoint's own counter is reset here. The caller resets the one
  // that tracks how far the simulation has advanced, because that is its.
  m_lastValidSequenceIdFromServer = -1;
}


void ClientToServer::RequestTeam(int _teamType, int _desiredId)
{
  DebugTrace("CLIENT : Requesting Team...\n");

  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetDesiredTeamId(_desiredId);
  letter->SetType(NetworkUpdate::UpdateType::RequestTeam);
  letter->SetTeamType(_teamType);
  SendLetter(std::move(letter));
}


void ClientToServer::SendIAmAlive(unsigned char _teamId, TeamControls const& _teamControls)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::Alive);
  letter->SetTeamId(_teamId);
  letter->SetWorldPos(_teamControls.m_mousePos);
  letter->SetTeamControls(_teamControls);
  SendLetter(std::move(letter));
}


void ClientToServer::SendSyncronisation(int _lastProcessedId, unsigned char _sync)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::Syncronise);
  letter->SetLastProcessedId(_lastProcessedId);
  letter->SetSync(_sync);
  SendLetter(std::move(letter));
}


void ClientToServer::RequestPause()
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::Pause);
  SendLetter(std::move(letter));
}


void ClientToServer::RequestSelectUnit(unsigned char _teamId, int _unitId, int _entityId, int _buildingId)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::SelectUnit);
  letter->SetTeamId(_teamId);
  letter->SetUnitId(_unitId);
  letter->SetEntityId(_entityId);
  letter->SetBuildingID(_buildingId);
  SendLetter(std::move(letter));
}


void ClientToServer::RequestCreateUnit(unsigned char _teamId, unsigned char _troopType, int _numToCreate, int _buildingId)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::CreateUnit);
  letter->SetTeamId(_teamId);
  letter->SetEntityType(_troopType);
  letter->SetNumTroops(_numToCreate);
  letter->SetBuildingID(_buildingId);
  SendLetter(std::move(letter));
}


void ClientToServer::RequestCreateUnit(unsigned char _teamId, unsigned char _troopType, int _numToCreate, DirectX::XMFLOAT3 const& _pos)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::CreateUnit);
  letter->SetTeamId(_teamId);
  letter->SetEntityType(_troopType);
  letter->SetNumTroops(_numToCreate);
  letter->SetBuildingID(-1);
  letter->SetWorldPos(_pos);
  SendLetter(std::move(letter));
}


void ClientToServer::RequestAimBuilding(unsigned char _teamId, int _buildingId, DirectX::XMFLOAT3 const& _pos)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::AimBuilding);
  letter->SetTeamId(_teamId);
  letter->SetBuildingID(_buildingId);
  letter->SetWorldPos(_pos);
  SendLetter(std::move(letter));
}


void ClientToServer::RequestToggleFence(int _buildingId)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::ToggleLaserFence);
  letter->SetBuildingID(_buildingId);
  SendLetter(std::move(letter));
}


void ClientToServer::RequestRunProgram(unsigned char _teamId, unsigned char _program)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::RunProgram);
  letter->SetTeamId(_teamId);
  letter->SetProgram(_program);
  SendLetter(std::move(letter));
}


void ClientToServer::RequestTargetProgram(unsigned char _teamId, unsigned char _program, DirectX::XMFLOAT3 const& _pos)
{
  auto letter = std::make_unique<NetworkUpdate>();
  letter->SetType(NetworkUpdate::UpdateType::TargetProgram);
  letter->SetTeamId(_teamId);
  letter->SetProgram(_program);
  letter->SetWorldPos(_pos);
  SendLetter(std::move(letter));
}
} // namespace Neuron
