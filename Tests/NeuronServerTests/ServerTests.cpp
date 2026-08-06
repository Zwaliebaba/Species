#include "pch.h"

#include "Generic.h"
#include "NetworkUpdate.h"
#include "ProtocolLimits.h"
#include "Server.h"
#include "ServerToClient.h"
#include "ServerToClientLetter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronServerTests
{
  // The authoritative host, moved here from NeuronCore by T9. This replaces
  // LayerWiringTests, which existed only to prove the project references were
  // intact while NeuronServer had no code of its own to test.
  //
  // What can be covered without a network is narrow, and worth being honest
  // about. Server::Initialise opens a socket on ServerPort, and Advance drains
  // it, so anything that runs a tick is still out of reach here — that is what
  // network-transport T7's transport seam is for. See docs/TESTING.md.
  //
  // What T4 brought into reach is ServerToClient, which used to open and
  // connect a socket of its own in order to exist at all and now holds an
  // address.
  //
  // What is reachable is the state a freshly constructed Server starts in, which
  // is worth pinning because the sequence id is the spine of the lockstep
  // protocol: clients buffer on it, the sync table is indexed by it, and a host
  // that started at 1 would desync every client on the first letter.
  TEST_CLASS(ServerTests)
  {
    public:
      TEST_METHOD(ANewServerStartsAtSequenceZero)
      {
        Server server;
        Assert::AreEqual(0, server.m_sequenceId);
      }

      TEST_METHOD(ANewServerHasNoClientsAndNoTeams)
      {
        Server server;
        Assert::AreEqual(0, server.m_clients.NumUsed());
        Assert::AreEqual(0, server.m_teams.NumUsed());
      }

      TEST_METHOD(AnUnknownAddressIsNotAClient)
      {
        // GetClientId walks the client DArray comparing dotted quads and returns
        // -1 for a miss. RegisterNewClient asserts on that -1, so a wrong answer
        // here would register the same client twice rather than fail loudly.
        Server server;
        char address[] = "127.0.0.1";
        Assert::AreEqual(-1, server.GetClientId(address));
      }

      TEST_METHOD(AServerThatWasNeverInitialisedCanStillBeDestroyed)
      {
        // Regression guard, and now a much weaker claim than it was: the
        // mutexes it was written about are gone with the listen thread, and the
        // socket closes itself whether or not it was ever opened. It stays
        // because every test above depends on it being true.
        {
          Server server;
        }
        Assert::IsTrue(true);
      }

      TEST_METHOD(AFullLetterCarriesTheRemainderIntoTheNextOne)
      {
        // The spillover contract, and the reason FillLetter is a free function:
        // Server::Advance itself needs a socket and a preferences file to run,
        // and this is the part of it that decides what a tick sends.
        //
        // 60 SelectUnits do not fit in one datagram. What used to happen is that
        // all 60 went in and the letter overran its buffer; what has to happen
        // is that the first letter takes what fits, the rest stay queued in
        // order, and the next letter takes them.
        std::vector<NetworkUpdate> pending;
        for (int i = 0; i < 60; ++i)
        {
          NetworkUpdate update;
          update.SetType(NetworkUpdate::UpdateType::SelectUnit);
          update.SetLastSequenceId(0);
          update.SetTeamId(1);
          update.SetUnitId(i);
          update.SetEntityId(0);
          update.SetBuildingID(-1);
          pending.push_back(update);
        }

        ServerToClientLetter first;
        first.SetType(ServerToClientLetter::LetterType::Update);
        first.SetSequenceId(1);
        FillLetter(first, pending);

        Assert::AreEqual(48, static_cast<int>(first.m_updates.size()));
        Assert::AreEqual(12, static_cast<int>(pending.size()));
        Assert::IsTrue(first.SerialisedSize() <= MaxDatagramSize);

        ServerToClientLetter second;
        second.SetType(ServerToClientLetter::LetterType::Update);
        second.SetSequenceId(2);
        FillLetter(second, pending);

        Assert::AreEqual(12, static_cast<int>(second.m_updates.size()));
        Assert::IsTrue(pending.empty());

        // In order, and with nothing lost or repeated across the two letters:
        // every client applies these in the order they arrive, so a reordering
        // here is a desync rather than a cosmetic difference.
        Assert::AreEqual(0, first.m_updates.front().m_unitId);
        Assert::AreEqual(47, first.m_updates.back().m_unitId);
        Assert::AreEqual(48, second.m_updates.front().m_unitId);
        Assert::AreEqual(59, second.m_updates.back().m_unitId);
      }

      TEST_METHOD(FillingAnEmptyQueueLeavesAnEmptyLetter)
      {
        // The ordinary case: an idle server still sends a letter every tick,
        // because that is what advances the sequence id for everyone.
        std::vector<NetworkUpdate> pending;

        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::LetterType::Update);
        FillLetter(letter, pending);

        Assert::IsTrue(letter.m_updates.empty());
        Assert::IsTrue(pending.empty());
      }

      TEST_METHOD(AClientsEndpointIsItsAddressOnTheClientPort)
      {
        // Constructing one of these used to open a socket and connect it, with
        // a DEBUG_ASSERT if that failed — so a client registry could not exist
        // without a network. It is an address now, and the address is the one
        // ConvertIPToInt produces, in the byte order sockaddr_in wants.
        char address[] = "127.0.0.1";
        ServerToClient client(address);

        // Compared as ints: the unit-test framework has no ToString for
        // unsigned long or unsigned short, so a mismatch would fail to compile
        // rather than fail the test.
        Assert::AreEqual(ConvertIPToInt(address), static_cast<int>(client.GetEndpoint().m_address));
        Assert::AreEqual(static_cast<int>(ClientPort), static_cast<int>(client.GetEndpoint().m_port));
        Assert::IsTrue(client.GetEndpoint().IsValid());
        Assert::AreEqual(-1, client.m_lastKnownSequenceId);
      }

      TEST_METHOD(AServerTeamRemembersWhichClientOwnsIt)
      {
        // The team registry maps a team back to the connection that asked for
        // it, which is how the host knows where to route and who to drop teams
        // for when a client leaves.
        ServerTeam team(3);
        Assert::AreEqual(3, team.m_clientId);
      }
  };
} // namespace NeuronServerTests
