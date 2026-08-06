#include "pch.h"

#include "LoopbackTransport.h"
#include "NetworkUpdate.h"
#include "Preferences.h"
#include "Profiler.h"
#include "ProtocolLimits.h"
#include "Server.h"
#include "ServerToClient.h"
#include "ServerToClientLetter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronServerTests
{
  // THE PROTOCOL HAS NEVER BEEN EXERCISED BY CI UNTIL THIS FILE.
  //
  // Everything else under Tests/ covers encodings — what a letter's bytes are,
  // and that they survive a round trip. Nothing covered the conversation those
  // letters make: a client joins and is told about it, asks for a team and is
  // given one, and every letter carries a sequence id the client acknowledges,
  // with anything unacknowledged sent again. That is where the defects live,
  // and it was unreachable because driving it meant opening a socket.
  //
  // network-transport T7 put Transport behind both endpoints, so it is reachable
  // now: a LoopbackTransport is a wire made of memory, and the Server under test
  // is the same Server the game runs, unchanged and unaware.
  //
  // WHAT THESE DO NOT TEST is the network. Delivery here is immediate, ordered
  // and lossless. Timing, reordering and real loss are what the Garden run and
  // an actual session are for; what is pinned here is the protocol's logic.
  TEST_CLASS(ServerProtocolTests)
  {
    private:
      // Server::Advance reads RecordDemo, and START_PROFILE dereferences
      // whatever it is handed, so a Server cannot be advanced without both of
      // these existing. Built once for the class rather than per test: a
      // PrefsManager with no file falls back to its built-in defaults, which is
      // exactly what the headless server does on a fresh machine.
      inline static Profiler* s_profiler = nullptr;
      inline static bool s_ownsPrefs = false;

      // The scripted client. It is not a ClientToServer — it is a hand-written
      // peer, so that what the server does is measured against the protocol
      // rather than against the other half of the same codebase agreeing with
      // itself.
      class ScriptedClient
      {
        public:
          ScriptedClient(Neuron::LoopbackNetwork& _network, unsigned short _port)
            : m_address(0x0100007F, _port), // 127.0.0.1
              m_transport(_network, m_address)
          {
          }

          Neuron::Endpoint const& Address() const { return m_address; }

          void Send(NetworkUpdate& _update, Neuron::Endpoint const& _server)
          {
            // A framed datagram, because that is what a client sends since T8 —
            // GetByteStream alone is the payload a letter carries nested, and
            // the server drops it as not being this protocol.
            char datagram[MaxDatagramSize];
            const int length = _update.SerialiseDatagram(datagram, static_cast<int>(sizeof(datagram)));
            Assert::IsTrue(length > 0, L"the scripted client should be able to serialise");
            Assert::IsFalse(static_cast<bool>(m_transport.Send(_server, datagram, length)), L"the scripted client should be able to send");
          }

          void SendJoin(Neuron::Endpoint const& _server)
          {
            NetworkUpdate update;
            update.SetType(NetworkUpdate::UpdateType::ClientJoin);
            update.SetLastSequenceId(-1);
            Send(update, _server);
          }

          void SendRequestTeam(Neuron::Endpoint const& _server)
          {
            NetworkUpdate update;
            update.SetType(NetworkUpdate::UpdateType::RequestTeam);
            update.SetLastSequenceId(-1);
            update.SetTeamType(0);
            update.SetEntityType(0);
            update.SetDesiredTeamId(-1);
            Send(update, _server);
          }

          // The acknowledgement. Every update a client sends carries the last
          // sequence id it has seen, and that is the whole of the ack protocol —
          // there is no separate message. SelectUnit is used because it is
          // broadcast rather than consumed by the server.
          void SendSelectUnit(Neuron::Endpoint const& _server, int _lastSequenceIdSeen, unsigned char _teamId, int _unitId)
          {
            NetworkUpdate update;
            update.SetType(NetworkUpdate::UpdateType::SelectUnit);
            update.SetLastSequenceId(_lastSequenceIdSeen);
            update.SetTeamId(_teamId);
            update.SetUnitId(_unitId);
            update.SetEntityId(0);
            update.SetBuildingID(-1);
            Send(update, _server);
          }

          // Everything waiting for this client, parsed.
          std::vector<ServerToClientLetter> Collect()
          {
            std::vector<ServerToClientLetter> letters;

            char datagram[MaxDatagramSize];
            int received = 0;
            Neuron::Endpoint from;

            while (m_transport.TryReceive(datagram, static_cast<int>(sizeof(datagram)), received, from))
              letters.push_back(ServerToClientLetter(datagram, received));

            return letters;
          }

        private:
          Neuron::Endpoint m_address;
          Neuron::LoopbackTransport m_transport;
      };

      // Counts the letters of one type in a batch.
      static int CountOf(std::vector<ServerToClientLetter> const& _letters, ServerToClientLetter::LetterType _type)
      {
        int count = 0;
        for (ServerToClientLetter const& letter : _letters)
        {
          if (letter.m_type == _type)
            ++count;
        }
        return count;
      }

    public:
      TEST_CLASS_INITIALIZE(CreateTheGlobalsAServerTickNeeds)
      {
        s_profiler = new Profiler();

        if (!g_prefsManager)
        {
          g_prefsManager = new PrefsManager("server-protocol-tests-preferences.txt");
          s_ownsPrefs = true;
        }
      }

      TEST_CLASS_CLEANUP(RemoveThem)
      {
        delete s_profiler;
        s_profiler = nullptr;

        if (s_ownsPrefs)
        {
          delete g_prefsManager;
          g_prefsManager = nullptr;
          s_ownsPrefs = false;
        }
      }

      TEST_METHOD(AJoinIsAnsweredWithHelloClient)
      {
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        ScriptedClient client(network, ClientPort);
        client.SendJoin(serverAddress);

        server.Advance();

        const std::vector<ServerToClientLetter> letters = client.Collect();

        Assert::AreEqual(1, CountOf(letters, ServerToClientLetter::LetterType::HelloClient),
                         L"a join should be answered with exactly one HelloClient");
        Assert::AreEqual(1, server.m_clients.NumUsed(), L"and the client should be registered");
      }

      TEST_METHOD(JoiningTwiceRegistersOneClient)
      {
        // RegisterNewClient asserts that the address is not already known, so a
        // duplicate join reaching it would be a Debug crash. Advance guards it;
        // this is what pins that guard.
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        ScriptedClient client(network, ClientPort);
        client.SendJoin(serverAddress);
        client.SendJoin(serverAddress);

        server.Advance();

        Assert::AreEqual(1, server.m_clients.NumUsed());
      }

      TEST_METHOD(RequestingATeamIsAnsweredWithTeamAssign)
      {
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        ScriptedClient client(network, ClientPort);
        client.SendJoin(serverAddress);
        server.Advance();
        client.Collect();

        client.SendRequestTeam(serverAddress);
        server.Advance();

        const std::vector<ServerToClientLetter> letters = client.Collect();

        Assert::AreEqual(1, CountOf(letters, ServerToClientLetter::LetterType::TeamAssign), L"a team request should be answered with one TeamAssign");
        Assert::AreEqual(1, server.m_teams.NumUsed());
      }

      TEST_METHOD(ATeamRequestFromAStrangerIsIgnored)
      {
        // RegisterNewTeam asserts the requester is a known client. Nothing but
        // Advance's guard stops an unjoined address reaching it.
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        ScriptedClient stranger(network, ClientPort);
        stranger.SendRequestTeam(serverAddress);

        server.Advance();

        Assert::AreEqual(0, server.m_teams.NumUsed());
      }

      TEST_METHOD(EveryTickAdvancesTheSequenceIdWhetherOrNotAnythingHappened)
      {
        // The sequence id is the spine of lockstep: clients buffer on it and
        // index the sync table by it, so an idle tick that failed to advance it
        // would stall every client rather than doing nothing.
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        Assert::AreEqual(0, server.m_sequenceId);

        server.Advance();
        server.Advance();
        server.Advance();

        Assert::AreEqual(3, server.m_sequenceId);
      }

      TEST_METHOD(UpdatesComeBackSequencedAndInOrder)
      {
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        ScriptedClient client(network, ClientPort);
        client.SendJoin(serverAddress);
        server.Advance();
        client.Collect();

        // Three ticks, one order each, acknowledging as it goes.
        for (int tick = 0; tick < 3; ++tick)
        {
          client.SendSelectUnit(serverAddress, server.m_sequenceId - 1, 1, 100 + tick);
          server.Advance();
        }

        const std::vector<ServerToClientLetter> letters = client.Collect();

        // Every letter the client is sent carries a sequence id, and they arrive
        // in ascending order with no gaps — that is what the client's inbox
        // release rule depends on.
        int previous = -1;
        int carried = 0;
        for (ServerToClientLetter const& letter : letters)
        {
          Assert::IsTrue(letter.GetSequenceId() > previous, L"sequence ids must ascend");
          previous = letter.GetSequenceId();
          carried += static_cast<int>(letter.m_updates.size());
        }

        Assert::AreEqual(3, carried, L"all three orders should have been broadcast");
      }

      TEST_METHOD(AnUnacknowledgedLetterIsSentAgain)
      {
        // The retransmission window, and the reason LoopbackNetwork can drop.
        // The server replays history from each client's last acknowledged
        // sequence id, so a letter that never arrives has to come again — this
        // is the mechanism that turned the 512-versus-1024 buffer mismatch into
        // a PERMANENT stall rather than one lost packet, because the letter that
        // could not be received could never be acknowledged either.
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        ScriptedClient client(network, ClientPort);
        client.SendJoin(serverAddress);
        server.Advance();
        client.Collect();

        // Acknowledge, so the replay window is empty and what follows is
        // unambiguous.
        client.SendSelectUnit(serverAddress, server.m_sequenceId - 1, 1, 1);
        server.Advance();

        std::vector<ServerToClientLetter> delivered = client.Collect();
        Assert::AreEqual(1, static_cast<int>(delivered.size()), L"an up-to-date client receives one letter per tick");
        const int lostSequenceId = delivered[0].GetSequenceId() + 1;

        // The next letter to this client is lost outright, and the client
        // acknowledges nothing from here on.
        network.DropNext(client.Address(), 1);

        std::vector<int> received;
        for (int tick = 0; tick < 2; ++tick)
        {
          server.Advance();
          for (ServerToClientLetter const& letter : client.Collect())
            received.push_back(letter.GetSequenceId());
        }

        Assert::AreEqual(1, network.Dropped(), L"exactly one letter should have been lost");

        const bool lostArrivedLater = std::ranges::find(received, lostSequenceId) != received.end();
        Assert::IsTrue(lostArrivedLater, L"the letter that was dropped must be sent again");
      }

      TEST_METHOD(AnAcknowledgedLetterIsNotSentAgain)
      {
        // The other half, and the one that bounds the server's work: once a
        // client says it has seen a sequence id, letters up to it stop being
        // replayed to it.
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        ScriptedClient client(network, ClientPort);
        client.SendJoin(serverAddress);
        server.Advance();
        server.Advance();
        client.Collect();

        // Acknowledge everything sent so far.
        const int acknowledged = server.m_sequenceId - 1;
        client.SendSelectUnit(serverAddress, acknowledged, 1, 7);
        server.Advance();
        const std::vector<ServerToClientLetter> afterAck = client.Collect();

        for (ServerToClientLetter const& letter : afterAck)
          Assert::IsTrue(letter.GetSequenceId() > acknowledged, L"nothing at or below the acknowledged id should be replayed");

        // And an idle tick after that sends exactly one new letter rather than
        // the whole history again.
        client.SendSelectUnit(serverAddress, server.m_sequenceId - 1, 1, 8);
        server.Advance();

        Assert::AreEqual(1, static_cast<int>(client.Collect().size()), L"an acknowledged client should receive one letter per tick");
      }

      TEST_METHOD(ASecondClientGetsItsOwnTeamAssign)
      {
        // Two clients on one host, which is exactly what today's addressing
        // cannot do: they are distinguished by source PORT, and the server keys
        // them by IP string and replies to a fixed port. Both therefore look
        // like one client to the server and both TeamAssigns go to the same
        // place.
        //
        // THIS TEST PINS THE CURRENT BEHAVIOUR AS A LIMITATION RATHER THAN AS
        // CORRECT. T9 is what fixes it, and it rewrites this test to assert
        // that each client receives its own.
        Neuron::LoopbackNetwork network;
        const Neuron::Endpoint serverAddress(0x0100007F, ServerPort);

        Server server;
        server.Initialise(s_profiler, std::make_unique<Neuron::LoopbackTransport>(network, serverAddress));

        ScriptedClient first(network, ClientPort);
        ScriptedClient second(network, ClientPort + 1);

        first.SendJoin(serverAddress);
        second.SendJoin(serverAddress);
        server.Advance();

        Assert::AreEqual(1, server.m_clients.NumUsed(), L"two clients on one host register as one, today");
        Assert::AreEqual(0, static_cast<int>(second.Collect().size()), L"and the second hears nothing at all");
      }
  };
} // namespace NeuronServerTests
