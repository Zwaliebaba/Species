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
  // Copying is used on the broadcast path. Server::Advance replays history to
  // each client with `std::make_unique<ServerToClientLetter>(*theLetter)`, so a
  // copy that dropped or aliased updates would corrupt what every client
  // receives while the original letter still looked correct. That used to be a
  // hand-written copy constructor with a "remember to update the copy
  // constructor" warning above the members; network-transport T2 made the
  // updates values and deleted both.
  TEST_CLASS(ServerToClientLetterTests)
  {
    private:
      static NetworkUpdate MakeSelectUnit(unsigned char _teamId, int _unitId)
      {
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::SelectUnit);
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
        letter.SetType(ServerToClientLetter::LetterType::HelloClient);
        letter.SetSequenceId(0x21436587);
        letter.SetConnectionId(3);
        letter.SetJoinToken(0x0BADF00D);

        char datagram[MaxDatagramSize];
        const int length = letter.Serialise(datagram, static_cast<int>(sizeof(datagram)));
        ByteReader reader(datagram, length);
        Assert::AreEqual(static_cast<int>(DatagramKind::ServerLetter), static_cast<int>(ReadDatagramHeader(reader)), L"every letter is framed");

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const int connectionId = reader.Read<int>();
        const int joinToken = reader.Read<int>();

        Assert::AreEqual(static_cast<int>(ServerToClientLetter::LetterType::HelloClient), type);
        Assert::AreEqual(0x21436587, sequenceId);
        Assert::AreEqual(3, connectionId);
        Assert::AreEqual(0x0BADF00D, joinToken, L"echoed so the joining client knows which welcome is its own");
        Assert::AreEqual(4 + 16, length, L"the frame, then the sixteen-byte payload");
      }

      TEST_METHOD(TeamAssignCarriesTeamIdTypeAndAddress)
      {
        // The letter that tells one specific client which team it owns. The ip
        // is what picks the recipient out of the broadcast.
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::LetterType::TeamAssign);
        letter.SetSequenceId(5);
        letter.SetTeamId(2);
        letter.SetTeamType(1);
        letter.SetConnectionId(7);

        char datagram[MaxDatagramSize];
        const int length = letter.Serialise(datagram, static_cast<int>(sizeof(datagram)));
        ByteReader reader(datagram, length);
        Assert::AreEqual(static_cast<int>(DatagramKind::ServerLetter), static_cast<int>(ReadDatagramHeader(reader)), L"every letter is framed");

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const unsigned char teamId = reader.Read<unsigned char>();
        const unsigned char teamType = reader.Read<unsigned char>();
        const int connectionId = reader.Read<int>();

        Assert::AreEqual(static_cast<int>(ServerToClientLetter::LetterType::TeamAssign), type);
        Assert::AreEqual(5, sequenceId);
        Assert::AreEqual(static_cast<unsigned char>(2), teamId);
        Assert::AreEqual(static_cast<unsigned char>(1), teamType);
        Assert::AreEqual(7, connectionId, L"which client this team belongs to, by id rather than by address");
        Assert::AreEqual(4 + 14, length, L"the frame, then the fourteen-byte payload");
      }

      TEST_METHOD(AnUpdateLetterCountsItsUpdatesBeforeCarryingThem)
      {
        // The count has to come first: the reader has no other way to know how
        // many NetworkUpdates follow, and they are variable width by type.
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::LetterType::Update);
        letter.SetSequenceId(9);

        NetworkUpdate first = MakeSelectUnit(1, 11);
        NetworkUpdate second = MakeSelectUnit(2, 22);
        Assert::IsTrue(letter.AddUpdate(first), L"the update should fit");
        Assert::IsTrue(letter.AddUpdate(second), L"the update should fit");

        char datagram[MaxDatagramSize];
        const int length = letter.Serialise(datagram, static_cast<int>(sizeof(datagram)));
        ByteReader reader(datagram, length);
        Assert::AreEqual(static_cast<int>(DatagramKind::ServerLetter), static_cast<int>(ReadDatagramHeader(reader)), L"every letter is framed");

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const int numUpdates = reader.Read<int>();

        Assert::AreEqual(static_cast<int>(ServerToClientLetter::LetterType::Update), type);
        Assert::AreEqual(9, sequenceId);
        Assert::AreEqual(2, numUpdates);

        // Frame is 4, header is 12, each SelectUnit is 21 — see NetworkUpdateTests.
        Assert::AreEqual(4 + 12 + 21 + 21, length);
      }

      TEST_METHOD(AnUpdateLetterSurvivesARoundTrip)
      {
        ServerToClientLetter sent;
        sent.SetType(ServerToClientLetter::LetterType::Update);
        sent.SetSequenceId(42);

        NetworkUpdate update = MakeSelectUnit(3, 77);
        Assert::IsTrue(sent.AddUpdate(update), L"the update should fit");

        char datagram[MaxDatagramSize];
        const int length = sent.Serialise(datagram, static_cast<int>(sizeof(datagram)));

        ServerToClientLetter received(datagram, length);

        Assert::AreEqual(static_cast<int>(ServerToClientLetter::LetterType::Update), static_cast<int>(received.m_type));
        Assert::AreEqual(42, received.GetSequenceId());
        Assert::AreEqual(1, static_cast<int>(received.m_updates.size()));
        Assert::AreEqual(static_cast<unsigned char>(3), received.m_updates[0].m_teamId);
        Assert::AreEqual(77, received.m_updates[0].m_unitId);
      }

      TEST_METHOD(AnEmptyUpdateLetterStillCarriesItsSequenceId)
      {
        // Server::Advance sends a letter every tick whether or not anything
        // happened, and that is what advances the sequence id for everyone. An
        // empty letter is the normal case on an idle server, not an edge case.
        ServerToClientLetter sent;
        sent.SetType(ServerToClientLetter::LetterType::Update);
        sent.SetSequenceId(101);

        char datagram[MaxDatagramSize];
        const int length = sent.Serialise(datagram, static_cast<int>(sizeof(datagram)));

        ServerToClientLetter received(datagram, length);

        Assert::AreEqual(101, received.GetSequenceId());
        Assert::AreEqual(0, static_cast<int>(received.m_updates.size()));
      }

      TEST_METHOD(ACopyCarriesEveryUpdateAsAValue)
      {
        // Server::Advance re-broadcasts from history with
        // `std::make_unique<ServerToClientLetter>(*theLetter)`. This used to be
        // a hand-written copy constructor over a vector of raw owning pointers,
        // and it was the reason the header carried a "remember to update the
        // copy constructor" warning. The updates are values now, so the
        // compiler's copy is the deep copy — what is left to check is that the
        // members still arrive, which is the thing that warning was about.
        ServerToClientLetter original;
        original.SetType(ServerToClientLetter::LetterType::Update);
        original.SetSequenceId(7);
        original.SetClientId(3);

        NetworkUpdate update = MakeSelectUnit(1, 55);
        Assert::IsTrue(original.AddUpdate(update), L"the update should fit");

        ServerToClientLetter copy(original);

        Assert::AreEqual(original.GetSequenceId(), copy.GetSequenceId());
        Assert::AreEqual(original.GetClientId(), copy.GetClientId());
        Assert::AreEqual(original.m_updates.size(), copy.m_updates.size());
        Assert::AreEqual(static_cast<int>(original.m_type), static_cast<int>(copy.m_type));
        Assert::AreEqual(55, copy.m_updates[0].m_unitId);
      }

      TEST_METHOD(AddUpdateStoresACopyRatherThanAReference)
      {
        // The server builds one NetworkUpdate, hands it to the letter, and
        // reuses it. A letter that aliased the caller's update instead of
        // copying it would broadcast whatever the server most recently built.
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::LetterType::Update);

        NetworkUpdate update = MakeSelectUnit(1, 55);
        Assert::IsTrue(letter.AddUpdate(update), L"the update should fit");
        update.SetUnitId(999);

        Assert::AreEqual(55, letter.m_updates[0].m_unitId);
      }

      TEST_METHOD(ACopyIsIndependentOfTheOriginal)
      {
        // The broadcast path stamps a client id onto each copy while the letter
        // in history keeps its own. If the copy shared state, every client would
        // be sent whichever id was written last.
        ServerToClientLetter original;
        original.SetType(ServerToClientLetter::LetterType::Update);
        original.SetSequenceId(7);
        original.SetClientId(1);

        NetworkUpdate update = MakeSelectUnit(1, 55);
        Assert::IsTrue(original.AddUpdate(update), L"the update should fit");

        ServerToClientLetter copy(original);
        copy.SetClientId(2);
        copy.m_updates[0].m_unitId = 999;

        Assert::AreEqual(1, original.GetClientId());
        Assert::AreEqual(2, copy.GetClientId());
        Assert::AreEqual(55, original.m_updates[0].m_unitId);
      }

      TEST_METHOD(AFullTickOfUpdatesFitsTheLetterBuffer)
      {
        // Serialise writes into a fixed 1024-byte buffer and only asserts the
        // bound in Debug. A busy tick with every client sending is the case that
        // would overrun it, so it is worth checking in Release too.
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::LetterType::Update);
        letter.SetSequenceId(1);

        // 48 SelectUnits at 21 bytes each is 1008, plus a 12-byte header.
        for (int i = 0; i < 48; ++i)
        {
          NetworkUpdate update = MakeSelectUnit(static_cast<unsigned char>(i % NUM_TEAMS), i);
          Assert::IsTrue(letter.AddUpdate(update), L"the update should fit");
        }

        char datagram[MaxDatagramSize];
        const int length = letter.Serialise(datagram, static_cast<int>(sizeof(datagram)));

        Assert::IsTrue(length <= MaxDatagramSize);
      }

      TEST_METHOD(TheUpdateThatWouldNotFitIsRefusedRatherThanWritten)
      {
        // The whole point of the cap. A 12-byte header plus 21 bytes per
        // SelectUnit means 48 fit in 1024 bytes and the 49th does not, so the
        // 49th has to be turned away — in Release as well as Debug. What stood
        // here was a DEBUG_ASSERT after Serialise had already written, which in
        // Release meant a letter longer than any datagram the receiver would
        // take, and therefore a sequence id no client could ever acknowledge.
        ServerToClientLetter letter;
        letter.SetType(ServerToClientLetter::LetterType::Update);
        letter.SetSequenceId(1);

        int accepted = 0;
        for (int i = 0; i < 60; ++i)
        {
          NetworkUpdate update = MakeSelectUnit(static_cast<unsigned char>(i % NUM_TEAMS), i);
          if (!letter.AddUpdate(update))
            break;
          ++accepted;
        }

        Assert::AreEqual(48, accepted);
        Assert::AreEqual(48, static_cast<int>(letter.m_updates.size()));

        char datagram[MaxDatagramSize];
        const int length = letter.Serialise(datagram, static_cast<int>(sizeof(datagram)));
        Assert::AreEqual(4 + 12 + 48 * 21, length);
        Assert::IsTrue(length <= MaxDatagramSize);
      }

      TEST_METHOD(TheSerialisedSizeMatchesWhatSerialiseWrites)
      {
        // SerialisedSize is what the cap is checked against, and it computes the
        // header from the letter's type rather than by serialising. That is a
        // second place the layout is written down, so it is worth one test that
        // fails the moment the two disagree — including when T8 adds a frame
        // header to Serialise and not to HeaderSize.
        char datagram[MaxDatagramSize];

        ServerToClientLetter hello;
        hello.SetType(ServerToClientLetter::LetterType::HelloClient);
        Assert::AreEqual(hello.Serialise(datagram, static_cast<int>(sizeof(datagram))), hello.SerialisedSize());

        ServerToClientLetter goodbye;
        goodbye.SetType(ServerToClientLetter::LetterType::GoodbyeClient);
        Assert::AreEqual(goodbye.Serialise(datagram, static_cast<int>(sizeof(datagram))), goodbye.SerialisedSize());

        ServerToClientLetter assign;
        assign.SetType(ServerToClientLetter::LetterType::TeamAssign);
        Assert::AreEqual(assign.Serialise(datagram, static_cast<int>(sizeof(datagram))), assign.SerialisedSize());

        ServerToClientLetter update;
        update.SetType(ServerToClientLetter::LetterType::Update);
        Assert::AreEqual(update.Serialise(datagram, static_cast<int>(sizeof(datagram))), update.SerialisedSize());

        for (int i = 0; i < 5; ++i)
        {
          NetworkUpdate one = MakeSelectUnit(1, i);
          Assert::IsTrue(update.AddUpdate(one), L"the update should fit");
          Assert::AreEqual(update.Serialise(datagram, static_cast<int>(sizeof(datagram))), update.SerialisedSize());
        }
      }

      // ------------------------------------------------------------------
      // What a datagram is allowed to do to the receiver.
      //
      // Every test below feeds this constructor bytes no correct sender would
      // produce. Until network-transport T1 each of them read past the end of
      // the receive buffer: the length parameter existed and was never
      // consulted, and numUpdates was taken from the wire and used as a loop
      // bound over updates that were not there. On a 512-byte stack buffer in
      // NetSocketListener that is a read of whatever followed it.
      //
      // The bar these set is "an empty or invalid letter, never a crash". They
      // are worth running under a sanitizer rather than only in the suite —
      // reading past a buffer is not something an assert can catch.
      // ------------------------------------------------------------------

      TEST_METHOD(ATruncatedHeaderYieldsAnInvalidLetter)
      {
        // Nine bytes: the frame, the letter type, and one byte of the sequence
        // id.
        const char truncated[9] = {'S', 'P', 2, 2, 4, 0, 0, 0, 9};

        const ServerToClientLetter letter(truncated, static_cast<int>(sizeof(truncated)));

        Assert::IsFalse(letter.IsValid());
        Assert::AreEqual(static_cast<int>(ServerToClientLetter::LetterType::Invalid), static_cast<int>(letter.m_type));
      }

      TEST_METHOD(ALetterCutShortAfterItsCountYieldsAnInvalidLetter)
      {
        // A well-formed Update letter with its updates cut off. This is the
        // shape a datagram takes when it is fragmented, forged, or — the case
        // that actually happens today — larger than the receiver's buffer.
        ServerToClientLetter sent;
        sent.SetType(ServerToClientLetter::LetterType::Update);
        sent.SetSequenceId(31);

        NetworkUpdate update = MakeSelectUnit(1, 5);
        Assert::IsTrue(sent.AddUpdate(update), L"the update should fit");

        char datagram[MaxDatagramSize];
        const int length = sent.Serialise(datagram, static_cast<int>(sizeof(datagram)));

        Assert::AreEqual(4 + 12 + 21, length);

        // Keep the frame and the 12-byte header, drop all but four bytes of the
        // 21-byte update.
        const ServerToClientLetter received(datagram, 20);

        Assert::IsFalse(received.IsValid());
      }

      TEST_METHOD(AnUpdateCountLargerThanTheDatagramStopsAtTheDatagram)
      {
        // The count is a claim, not a fact. 100,000 updates in a 12-byte
        // datagram used to be 100,000 iterations of reading 8+ bytes each from
        // wherever the receive buffer sat in memory.
        char datagram[16] = {};
        ByteWriter writer(datagram, sizeof(datagram));
        WriteDatagramHeader(writer, DatagramKind::ServerLetter);
        writer.Write<int>(static_cast<int>(ServerToClientLetter::LetterType::Update));
        writer.Write<int>(77);
        writer.Write<int>(100000);

        const ServerToClientLetter letter(datagram, static_cast<int>(sizeof(datagram)));

        Assert::IsFalse(letter.IsValid());
        Assert::AreEqual(0, static_cast<int>(letter.m_updates.size()));
      }

      TEST_METHOD(ANegativeUpdateCountYieldsAnInvalidLetter)
      {
        // A DEBUG_ASSERT stood here, which is not a check: in Release the loop
        // ran with a negative bound, and in Debug a hostile datagram could stop
        // the build a developer was running.
        char datagram[16] = {};
        ByteWriter writer(datagram, sizeof(datagram));
        WriteDatagramHeader(writer, DatagramKind::ServerLetter);
        writer.Write<int>(static_cast<int>(ServerToClientLetter::LetterType::Update));
        writer.Write<int>(77);
        writer.Write<int>(-1);

        const ServerToClientLetter letter(datagram, static_cast<int>(sizeof(datagram)));

        Assert::IsFalse(letter.IsValid());
        Assert::AreEqual(0, static_cast<int>(letter.m_updates.size()));
      }

      TEST_METHOD(ALetterTypeThisBuildDoesNotKnowIsInvalid)
      {
        // Nothing frames these datagrams yet, so the first four bytes of
        // anything arriving on the port are read as a letter type. T8 adds the
        // magic and version that let a receiver tell this protocol from the
        // rest of the internet; until then, an unrecognised type must at least
        // not be acted on.
        char datagram[16] = {};
        ByteWriter writer(datagram, sizeof(datagram));
        WriteDatagramHeader(writer, DatagramKind::ServerLetter);
        writer.Write<int>(4242);
        writer.Write<int>(1);
        writer.Write<int>(0);

        const ServerToClientLetter letter(datagram, static_cast<int>(sizeof(datagram)));

        Assert::IsFalse(letter.IsValid());
      }

      TEST_METHOD(AZeroLengthDatagramYieldsAnInvalidLetter)
      {
        char datagram[1] = {};

        const ServerToClientLetter letter(datagram, 0);

        Assert::IsFalse(letter.IsValid());
      }

      TEST_METHOD(TheLengthParameterBoundsTheParseRatherThanTheBuffer)
      {
        // The regression this pins directly: a letter serialised into a longer
        // buffer, then handed over with a length that stops mid-update. What
        // follows in the buffer is real, readable, correctly-formatted data —
        // so a parse that ignored _len would succeed and report a letter that
        // never arrived.
        ServerToClientLetter sent;
        sent.SetType(ServerToClientLetter::LetterType::Update);
        sent.SetSequenceId(64);

        NetworkUpdate first = MakeSelectUnit(1, 11);
        NetworkUpdate second = MakeSelectUnit(2, 22);
        Assert::IsTrue(sent.AddUpdate(first), L"the update should fit");
        Assert::IsTrue(sent.AddUpdate(second), L"the update should fit");

        char datagram[MaxDatagramSize];
        const int length = sent.Serialise(datagram, static_cast<int>(sizeof(datagram)));
        Assert::AreEqual(4 + 12 + 21 + 21, length);

        // Everything is present in the buffer; only _len says otherwise.
        const ServerToClientLetter received(datagram, 4 + 12 + 21);

        Assert::IsFalse(received.IsValid());
      }

      TEST_METHOD(AnUpdateTypeThisBuildDoesNotKnowInvalidatesTheLetter)
      {
        // The same question one level down: the letter is well-formed and the
        // update inside it is not. Fewer updates arriving than the count
        // promised means the rest of the datagram cannot be located, so the
        // letter goes rather than arriving short.
        char datagram[24] = {};
        ByteWriter writer(datagram, sizeof(datagram));
        WriteDatagramHeader(writer, DatagramKind::ServerLetter);
        writer.Write<int>(static_cast<int>(ServerToClientLetter::LetterType::Update));
        writer.Write<int>(5);
        writer.Write<int>(1);
        writer.Write<int>(999); // update type
        writer.Write<int>(0);   // update sequence id

        const ServerToClientLetter letter(datagram, static_cast<int>(sizeof(datagram)));

        Assert::IsFalse(letter.IsValid());
        Assert::AreEqual(0, static_cast<int>(letter.m_updates.size()));
      }

      // Same reason as TheUpdateTypeValuesAreTheProtocol in NetworkUpdateTests:
      // m_type goes out as four bytes and comes back as four bytes, so these five
      // numbers are what a client and a server agree on. Five enumerators is a
      // small enough set that inserting one in the middle looks harmless.
      TEST_METHOD(TheLetterTypeValuesAreTheProtocol)
      {
        Assert::AreEqual(0, static_cast<int>(ServerToClientLetter::LetterType::Invalid));
        Assert::AreEqual(1, static_cast<int>(ServerToClientLetter::LetterType::HelloClient));
        Assert::AreEqual(2, static_cast<int>(ServerToClientLetter::LetterType::GoodbyeClient));
        Assert::AreEqual(3, static_cast<int>(ServerToClientLetter::LetterType::TeamAssign));
        Assert::AreEqual(4, static_cast<int>(ServerToClientLetter::LetterType::Update));
      }
  };
} // namespace NeuronCoreTests
