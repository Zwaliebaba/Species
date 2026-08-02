#include "pch.h"

#include "Server.h"
#include "ServerToClient.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronServerTests
{
  // The authoritative host, moved here from NeuronCore by T9. This replaces
  // LayerWiringTests, which existed only to prove the project references were
  // intact while NeuronServer had no code of its own to test.
  //
  // What can be covered without a network is narrow, and worth being honest
  // about. Server::Initialise creates the mutexes, opens a NetLib and starts a
  // listen thread on port 4000, and ServerToClient's constructor connects a
  // socket in order to exist at all. So anything touching a client connection,
  // the inbox, the outbox or sequencing is out of reach here and stays that way
  // until those are separated from their transport — which is a task, not an
  // oversight. See docs/TESTING.md.
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
        // Regression guard. The mutexes are created by Initialise, not by the
        // constructor, and the destructor used to lock them unconditionally — so
        // constructing a Server without initialising it and letting it fall out
        // of scope dereferenced null. Species always pairs the two calls, so
        // nothing in the game hit it; every test above does.
        {
          Server server;
        }
        Assert::IsTrue(true);
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
