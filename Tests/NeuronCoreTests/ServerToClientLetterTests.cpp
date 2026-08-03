#include "pch.h"

#include "ByteStream.h"
#include "NetworkUpdate.h"
#include "ProtocolLimits.h"
#include "ServerToClientLetter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // ServerToClientLetter is the server-to-client half of the wire format, and it
  // had no coverage at all — NetworkUpdate carries what one client did, this
  // carries what the server decided and broadcasts to everyone.
  //
  // Two things here are load-bearing beyond the byte layout.
  //
  // The sequence id is the spine of deterministic lockstep: clients buffer
  // out-of-order arrivals on it, release a letter only when it is the next one
  // expected, and index the sync table by it. It rides in the header of every
  // letter type.
  //
  // The copy constructor is used on the broadcast path. Server::Advance replays
  // history to each client with `new ServerToClientLetter(*theLetter)`, so a copy
  // that dropped or aliased updates would corrupt what every client receives
  // while the original letter still looked correct. The header carries the
  // warning: "If you add any new data here, remember to update the copy
  // constructor" — that is the failure this pins.
  TEST_CLASS(ServerToClientLetterTests)
  {
    private:
      static NetworkUpdate MakeSelectUnit(unsigned char _teamId, int _unitId)
      {
        NetworkUpdate update;
        update.SetType(NetworkUpdate::SelectUnit);
        update.SetLastSequenceId(0);
        update.SetTeamId(_teamId);
        update.SetUnitId(_unitId);
        update.SetEntityId(0);
        update.SetBuildingID(-1);
        return update;
      }

    public:
      TEST_METHOD(TheHeaderIsATypeThenASequenceId)
      {
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::HelloClient);
        letter.SetSequenceId(0x21436587);
        letter.SetIp(0x0100007F);

        int length = 0;
        char* read = letter.GetByteStream(&length);

        const int type = READ_INT(read);
        const int sequenceId = READ_INT(read);
        const int ip = READ_INT(read);

        Assert::AreEqual(static_cast<int>(ServerToClientLetter::HelloClient), type);
        Assert::AreEqual(0x21436587, sequenceId);
        Assert::AreEqual(0x0100007F, ip);
        Assert::AreEqual(12, length);
      }

      TEST_METHOD(TeamAssignCarriesTeamIdTypeAndAddress)
      {
        // The letter that tells one specific client which team it owns. The ip
        // is what picks the recipient out of the broadcast.
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::TeamAssign);
        letter.SetSequenceId(5);
        letter.SetTeamId(2);
        letter.SetTeamType(1);
        letter.SetIp(0x0100007F);

        int length = 0;
        char* read = letter.GetByteStream(&length);

        const int type = READ_INT(read);
        const int sequenceId = READ_INT(read);
        const unsigned char teamId = READ_UNSIGNED_CHAR(read);
        const unsigned char teamType = READ_UNSIGNED_CHAR(read);
        const int ip = READ_INT(read);

        Assert::AreEqual(static_cast<int>(ServerToClientLetter::TeamAssign), type);
        Assert::AreEqual(5, sequenceId);
        Assert::AreEqual(static_cast<unsigned char>(2), teamId);
        Assert::AreEqual(static_cast<unsigned char>(1), teamType);
        Assert::AreEqual(0x0100007F, ip);
        Assert::AreEqual(14, length);
      }

      TEST_METHOD(AnUpdateLetterCountsItsUpdatesBeforeCarryingThem)
      {
        // The count has to come first: the reader has no other way to know how
        // many NetworkUpdates follow, and they are variable width by type.
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::Update);
        letter.SetSequenceId(9);

        NetworkUpdate first = MakeSelectUnit(1, 11);
        NetworkUpdate second = MakeSelectUnit(2, 22);
        letter.AddUpdate(&first);
        letter.AddUpdate(&second);

        int length = 0;
        char* read = letter.GetByteStream(&length);

        const int type = READ_INT(read);
        const int sequenceId = READ_INT(read);
        const int numUpdates = READ_INT(read);

        Assert::AreEqual(static_cast<int>(ServerToClientLetter::Update), type);
        Assert::AreEqual(9, sequenceId);
        Assert::AreEqual(2, numUpdates);

        // Header is 12 bytes, each SelectUnit is 21 — see NetworkUpdateTests.
        Assert::AreEqual(12 + 21 + 21, length);
      }

      TEST_METHOD(AnUpdateLetterSurvivesARoundTrip)
      {
        ServerToClientLetter sent;
        sent.SetType(ServerToClientLetter::Update);
        sent.SetSequenceId(42);

        NetworkUpdate update = MakeSelectUnit(3, 77);
        sent.AddUpdate(&update);

        int length = 0;
        char* stream = sent.GetByteStream(&length);

        ServerToClientLetter received(stream, length);

        Assert::AreEqual(static_cast<int>(ServerToClientLetter::Update), static_cast<int>(received.m_type));
        Assert::AreEqual(42, received.GetSequenceId());
        Assert::AreEqual(1, static_cast<int>(received.m_updates.size()));
        Assert::AreEqual(static_cast<unsigned char>(3), received.m_updates[0]->m_teamId);
        Assert::AreEqual(77, received.m_updates[0]->m_unitId);
      }

      TEST_METHOD(AnEmptyUpdateLetterStillCarriesItsSequenceId)
      {
        // Server::Advance sends a letter every tick whether or not anything
        // happened, and that is what advances the sequence id for everyone. An
        // empty letter is the normal case on an idle server, not an edge case.
        ServerToClientLetter sent;
        sent.SetType(ServerToClientLetter::Update);
        sent.SetSequenceId(101);

        int length = 0;
        char* stream = sent.GetByteStream(&length);

        ServerToClientLetter received(stream, length);

        Assert::AreEqual(101, received.GetSequenceId());
        Assert::AreEqual(0, static_cast<int>(received.m_updates.size()));
      }

      TEST_METHOD(TheCopyConstructorDeepCopiesEveryUpdate)
      {
        // Server::Advance re-broadcasts from history with
        // `new ServerToClientLetter(*theLetter)`. A shallow copy would hand two
        // letters the same NetworkUpdate pointers, and the first destructor to
        // run would leave the other pointing at freed memory.
        ServerToClientLetter original;
        original.SetType(ServerToClientLetter::Update);
        original.SetSequenceId(7);
        original.SetClientId(3);

        NetworkUpdate update = MakeSelectUnit(1, 55);
        original.AddUpdate(&update);

        ServerToClientLetter copy(original);

        Assert::AreEqual(original.GetSequenceId(), copy.GetSequenceId());
        Assert::AreEqual(original.GetClientId(), copy.GetClientId());
        Assert::AreEqual(original.m_updates.size(), copy.m_updates.size());

        Assert::IsTrue(original.m_updates[0] != copy.m_updates[0], L"the copy must own its own updates, not share pointers");
        Assert::AreEqual(55, copy.m_updates[0]->m_unitId);
      }

      TEST_METHOD(ACopyIsIndependentOfTheOriginal)
      {
        // The broadcast path stamps a client id onto each copy while the letter
        // in history keeps its own. If the copy shared state, every client would
        // be sent whichever id was written last.
        ServerToClientLetter original;
        original.SetType(ServerToClientLetter::Update);
        original.SetSequenceId(7);
        original.SetClientId(1);

        NetworkUpdate update = MakeSelectUnit(1, 55);
        original.AddUpdate(&update);

        ServerToClientLetter copy(original);
        copy.SetClientId(2);
        copy.m_updates[0]->m_unitId = 999;

        Assert::AreEqual(1, original.GetClientId());
        Assert::AreEqual(2, copy.GetClientId());
        Assert::AreEqual(55, original.m_updates[0]->m_unitId);
      }

      TEST_METHOD(AFullTickOfUpdatesFitsTheLetterBuffer)
      {
        // GetByteStream writes into a fixed 1024-byte buffer and only asserts
        // the bound in Debug. A busy tick with every client sending is the case
        // that would overrun it, so it is worth checking in Release too.
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::Update);
        letter.SetSequenceId(1);

        // 48 SelectUnits at 21 bytes each is 1008, plus a 12-byte header.
        for (int i = 0; i < 48; ++i)
        {
          NetworkUpdate update = MakeSelectUnit(static_cast<unsigned char>(i % NUM_TEAMS), i);
          letter.AddUpdate(&update);
        }

        int length = 0;
        letter.GetByteStream(&length);

        Assert::IsTrue(length < SERVERTOCLIENTLETTER_BYTESTREAMSIZE);
      }

      // Same reason as TheUpdateTypeValuesAreTheProtocol in NetworkUpdateTests:
      // m_type goes out as WRITE_INT and comes back as READ_INT, so these five
      // numbers are what a client and a server agree on. Five enumerators is a
      // small enough set that inserting one in the middle looks harmless.
      TEST_METHOD(TheLetterTypeValuesAreTheProtocol)
      {
        Assert::AreEqual(0, static_cast<int>(ServerToClientLetter::Invalid));
        Assert::AreEqual(1, static_cast<int>(ServerToClientLetter::HelloClient));
        Assert::AreEqual(2, static_cast<int>(ServerToClientLetter::GoodbyeClient));
        Assert::AreEqual(3, static_cast<int>(ServerToClientLetter::TeamAssign));
        Assert::AreEqual(4, static_cast<int>(ServerToClientLetter::Update));
      }
  };
} // namespace NeuronCoreTests
