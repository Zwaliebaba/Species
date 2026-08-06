#include "pch.h"

#include "ByteStream.h"
#include "NetworkUpdate.h"
#include "ProtocolLimits.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // NetworkUpdate is the client-to-server half of the wire format. Every packet
  // the client sends is one of these, built field by field by ClientToServer's
  // Request*/Send* methods and flattened by GetByteStream.
  //
  // These tests exist because T12 moved the game-facing half of ClientToServer
  // into Species while leaving the packet builders where they were, and the
  // claim attached to that move is "the wire format is unchanged". That is a
  // claim about bytes, so it is pinned in bytes here rather than asserted in a
  // commit message. Determinism carries the same requirement for the same
  // reason — see CODING_STANDARDS.md.
  //
  // The builders themselves are not called directly: constructing a
  // ClientToServer opens sockets and starts a listen thread. What is pinned
  // instead is the field set each builder fills in, which is the part that
  // reaches the wire.
  //
  // Every read below goes through a local rather than being inlined into the
  // Assert, which is how these were written when reading meant a two-statement
  // macro that could not sit inside a call. It is kept because it puts the field
  // ORDER in the source in the order the wire has it, which is the thing under
  // test.
  TEST_CLASS(NetworkUpdateTests)
  {
    private:
      // Fills every field the client-sendable types serialise, so no test reads
      // an indeterminate member into a packet.
      static void FillAllFields(NetworkUpdate& _update)
      {
        _update.SetLastSequenceId(1);
        _update.SetTeamType(1);
        _update.SetDesiredTeamId(-1);
        _update.SetTeamId(1);
        _update.SetEntityType(1);
        _update.SetNumTroops(1);
        _update.SetUnitId(1);
        _update.SetEntityId(1);
        _update.SetBuildingID(1);
        _update.SetProgram(1);
        _update.SetLastProcessedId(1);
        _update.SetSync(1);
        // Controls before position: SetTeamControls would otherwise overwrite it.
        _update.SetTeamControls(TeamControls());
        _update.SetWorldPos(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
      }

    public:
      TEST_METHOD(TheHeaderIsATypeThenASequenceId)
      {
        // Every type shares this prefix, so two builds that disagree about it
        // disagree about every packet. ClientLeave is used because it is the
        // prefix and nothing else — ClientJoin gained a join token in
        // network-transport T9 and is pinned by AJoinCarriesItsToken below.
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::ClientLeave);
        update.SetLastSequenceId(0x11223344);

        int length = 0;
        char* stream = update.GetByteStream(&length);
        ByteReader reader(stream, length);

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::ClientLeave), type);
        Assert::AreEqual(0x11223344, sequenceId);
        Assert::AreEqual(8, length);
      }

      TEST_METHOD(AJoinCarriesItsToken)
      {
        // The only client-chosen value in the protocol, and the only field a
        // join carries. The server echoes it in HelloClient so the joining
        // client can pick its own welcome out of a stream every client
        // receives — see ServerToClientLetter.h.
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::ClientJoin);
        update.SetLastSequenceId(7);
        update.SetJoinToken(0x0BADF00D);

        int length = 0;
        char* stream = update.GetByteStream(&length);
        ByteReader reader(stream, length);

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const int joinToken = reader.Read<int>();

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::ClientJoin), type);
        Assert::AreEqual(7, sequenceId);
        Assert::AreEqual(0x0BADF00D, joinToken);
        Assert::AreEqual(12, length, L"eight bytes of header and the token");
      }

      TEST_METHOD(SelectUnitCarriesTeamUnitEntityAndBuilding)
      {
        // The field set RequestSelectUnit fills. m_unitId, m_entityId and
        // m_buildingId are raw DArray slots — network identity rather than
        // opaque handles — so their order and width are load-bearing.
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::SelectUnit);
        update.SetLastSequenceId(-1);
        update.SetTeamId(2);
        update.SetUnitId(7);
        update.SetEntityId(11);
        update.SetBuildingID(13);

        int length = 0;
        char* stream = update.GetByteStream(&length);
        ByteReader reader(stream, length);

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const unsigned char teamId = reader.Read<unsigned char>();
        const int unitId = reader.Read<int>();
        const int entityId = reader.Read<int>();
        const int buildingId = reader.Read<int>();

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::SelectUnit), type);
        Assert::AreEqual(-1, sequenceId);
        Assert::AreEqual(static_cast<unsigned char>(2), teamId);
        Assert::AreEqual(7, unitId);
        Assert::AreEqual(11, entityId);
        Assert::AreEqual(13, buildingId);
        Assert::AreEqual(21, length);
      }

      TEST_METHOD(CreateUnitCarriesItsPositionAfterTheCounts)
      {
        // RequestCreateUnit has two overloads — one naming a factory, one naming
        // a position — and both go out as this single layout.
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::CreateUnit);
        update.SetLastSequenceId(4);
        update.SetTeamId(1);
        update.SetEntityType(3);
        update.SetNumTroops(20);
        update.SetBuildingID(-1);
        update.SetWorldPos(DirectX::XMFLOAT3(1.0f, 2.0f, 3.0f));

        int length = 0;
        char* stream = update.GetByteStream(&length);
        ByteReader reader(stream, length);

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const unsigned char teamId = reader.Read<unsigned char>();
        const unsigned char entityType = reader.Read<unsigned char>();
        const int numTroops = reader.Read<int>();
        const int buildingId = reader.Read<int>();
        const float x = reader.Read<float>();
        const float y = reader.Read<float>();
        const float z = reader.Read<float>();

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::CreateUnit), type);
        Assert::AreEqual(4, sequenceId);
        Assert::AreEqual(static_cast<unsigned char>(1), teamId);
        Assert::AreEqual(static_cast<unsigned char>(3), entityType);
        Assert::AreEqual(20, numTroops);
        Assert::AreEqual(-1, buildingId);
        Assert::AreEqual(1.0f, x);
        Assert::AreEqual(2.0f, y);
        Assert::AreEqual(3.0f, z);
        Assert::AreEqual(30, length);
      }

      TEST_METHOD(AliveCarriesTheTeamControlFlagsAsOneWord)
      {
        // SendIAmAlive is the highest-frequency packet — one per client per
        // IAMALIVE_PERIOD. TeamControls goes out through GetFlags, so the bit
        // assignment in TeamControls.cpp is part of the wire format.
        //
        // Built in the order SendIAmAlive builds it, which matters more than it
        // looks — see TheWorldPositionIsTheTeamControlsMousePosition below.
        TeamControls controls;
        controls.m_mousePos = DirectX::XMFLOAT3(-4.5f, 0.0f, 4.5f);
        controls.m_unitMove = 1;
        controls.m_cameraEntityTracking = 1;

        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::Alive);
        update.SetLastSequenceId(9);
        update.SetTeamId(0);
        update.SetWorldPos(controls.m_mousePos);
        update.SetTeamControls(controls);
        update.SetSync(200);

        int length = 0;
        char* stream = update.GetByteStream(&length);
        ByteReader reader(stream, length);

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const unsigned char teamId = reader.Read<unsigned char>();
        const float x = reader.Read<float>();
        const float y = reader.Read<float>();
        const float z = reader.Read<float>();
        const unsigned short flags = reader.Read<unsigned short>();
        const unsigned char sync = reader.Read<unsigned char>();

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::Alive), type);
        Assert::AreEqual(9, sequenceId);
        Assert::AreEqual(static_cast<unsigned char>(0), teamId);
        Assert::AreEqual(-4.5f, x);
        Assert::AreEqual(0.0f, y);
        Assert::AreEqual(4.5f, z);
        Assert::AreEqual(static_cast<int>(0x0001 | 0x0040), static_cast<int>(flags));
        Assert::AreEqual(static_cast<unsigned char>(200), sync);
        Assert::AreEqual(24, length);
      }

      TEST_METHOD(TheWorldPositionIsTheTeamControlsMousePosition)
      {
        // Not a separate field. Both GetWorldPos overloads return
        // m_teamControls.m_mousePos, and SetWorldPos assigns through one of
        // them — "Shared with m_teamControls", as NetworkUpdate.cpp puts it.
        //
        // The consequence is a trap: SetTeamControls after SetWorldPos discards
        // the position, silently, because it copies the whole struct back over
        // it. SendIAmAlive gets away with calling both because it passes
        // _teamControls.m_mousePos to the first one. Any other caller ordering
        // them that way sends a zero position and nothing says so.
        //
        // This test exists to make that fact fail loudly if the aliasing is
        // ever removed, and to document it where the next person will look.
        NetworkUpdate update;
        update.SetWorldPos(DirectX::XMFLOAT3(1.0f, 2.0f, 3.0f));
        Assert::AreEqual(1.0f, update.m_teamControls.m_mousePos.x);

        TeamControls empty;
        update.SetTeamControls(empty);
        Assert::AreEqual(0.0f, update.GetWorldPos().x);
      }

      // The remaining two update types that carry a world position, pinned at
      // their byte offsets before directxmath-migration T9 turns that position
      // into an XMFLOAT3. Alive and CreateUnit were already pinned above.
      //
      // XMFLOAT3 has the same layout as Vector3, so the packet should not move
      // — but "should not" is not how a wire format is verified, and this is the
      // one part of that migration that would break other machines rather than
      // just this build. If T9 changes a byte, these fail; they are not tests to
      // update afterwards.
      TEST_METHOD(AimBuildingCarriesItsPositionAfterTheBuildingId)
      {
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::AimBuilding);
        update.SetLastSequenceId(11);
        update.SetTeamId(2);
        update.SetBuildingID(37);
        update.SetWorldPos(DirectX::XMFLOAT3(-1.5f, 64.0f, 2.25f));

        int length = 0;
        char* stream = update.GetByteStream(&length);
        ByteReader reader(stream, length);

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const unsigned char teamId = reader.Read<unsigned char>();
        const int buildingId = reader.Read<int>();
        const float x = reader.Read<float>();
        const float y = reader.Read<float>();
        const float z = reader.Read<float>();

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::AimBuilding), type);
        Assert::AreEqual(11, sequenceId);
        Assert::AreEqual(static_cast<unsigned char>(2), teamId);
        Assert::AreEqual(37, buildingId);
        Assert::AreEqual(-1.5f, x);
        Assert::AreEqual(64.0f, y);
        Assert::AreEqual(2.25f, z);
        Assert::AreEqual(25, length);
      }

      TEST_METHOD(TargetProgramCarriesItsPositionAfterTheProgram)
      {
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::TargetProgram);
        update.SetLastSequenceId(12);
        update.SetTeamId(1);
        update.SetProgram(5);
        update.SetWorldPos(DirectX::XMFLOAT3(10.25f, -0.5f, 300.0f));

        int length = 0;
        char* stream = update.GetByteStream(&length);
        ByteReader reader(stream, length);

        const int type = reader.Read<int>();
        const int sequenceId = reader.Read<int>();
        const unsigned char teamId = reader.Read<unsigned char>();
        const unsigned char program = reader.Read<unsigned char>();
        const float x = reader.Read<float>();
        const float y = reader.Read<float>();
        const float z = reader.Read<float>();

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::TargetProgram), type);
        Assert::AreEqual(12, sequenceId);
        Assert::AreEqual(static_cast<unsigned char>(1), teamId);
        Assert::AreEqual(static_cast<unsigned char>(5), program);
        Assert::AreEqual(10.25f, x);
        Assert::AreEqual(-0.5f, y);
        Assert::AreEqual(300.0f, z);
        Assert::AreEqual(22, length);
      }

      TEST_METHOD(EveryClientPacketFitsTheFixedBuffer)
      {
        // NETWORKUPDATE_BYTESTREAMSIZE is 42, and GetByteStream only asserts
        // against it in Debug. Overrunning m_byteStream is a memory bug rather
        // than a protocol one, so it is worth checking every type the client can
        // send in both configurations.
        const NetworkUpdate::UpdateType clientTypes[] = {
          NetworkUpdate::UpdateType::ClientJoin,  NetworkUpdate::UpdateType::ClientLeave,
          NetworkUpdate::UpdateType::RequestTeam, NetworkUpdate::UpdateType::Alive,
          NetworkUpdate::UpdateType::SelectUnit,  NetworkUpdate::UpdateType::CreateUnit,
          NetworkUpdate::UpdateType::AimBuilding, NetworkUpdate::UpdateType::ToggleLaserFence,
          NetworkUpdate::UpdateType::RunProgram,  NetworkUpdate::UpdateType::TargetProgram,
          NetworkUpdate::UpdateType::Pause,       NetworkUpdate::UpdateType::Syncronise,
        };

        for (const NetworkUpdate::UpdateType type : clientTypes)
        {
          NetworkUpdate update;
          FillAllFields(update);
          update.SetType(type);

          int length = 0;
          update.GetByteStream(&length);

          Assert::IsTrue(length >= 8);
          Assert::IsTrue(length < NETWORKUPDATE_BYTESTREAMSIZE);
        }
      }

      TEST_METHOD(AnUpdateSurvivesARoundTripThroughItsOwnStream)
      {
        // ReadByteStream is what the far end runs on the bytes GetByteStream
        // produced. The two have to stay in step field for field.
        NetworkUpdate sent;
        sent.SetType(NetworkUpdate::UpdateType::TargetProgram);
        sent.SetLastSequenceId(77);
        sent.SetTeamId(3);
        sent.SetProgram(5);
        sent.SetWorldPos(DirectX::XMFLOAT3(10.25f, -0.5f, 300.0f));

        char datagram[MaxDatagramSize];
        const int length = sent.SerialiseDatagram(datagram, static_cast<int>(sizeof(datagram)));

        NetworkUpdate received(datagram, length);

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::TargetProgram), static_cast<int>(received.m_type));
        Assert::AreEqual(77, received.m_lastSequenceId);
        Assert::AreEqual(static_cast<unsigned char>(3), received.m_teamId);
        Assert::AreEqual(static_cast<unsigned char>(5), received.m_program);
        Assert::AreEqual(10.25f, received.GetWorldPos().x);
        Assert::AreEqual(-0.5f, received.GetWorldPos().y);
        Assert::AreEqual(300.0f, received.GetWorldPos().z);
      }

      TEST_METHOD(AllNineTeamControlFlagsSurviveARoundTrip)
      {
        // Regression guard for a flag that used to be lost in transit.
        //
        // TeamControls has nine flags and GetFlags puts the ninth,
        // m_endSetTarget, at 0x0100. The wire always carried both bytes, but
        // ReadByteStream read them into an unsigned char, so that bit was
        // discarded on receive. The cursor still advanced two bytes, so packets
        // stayed aligned and nothing downstream ever complained — the flag just
        // silently arrived clear.
        //
        // The server made it worse than a client-side glitch: it deserialises
        // each update, then re-serialises it into the letter it broadcasts, so
        // the bit was destroyed at the hop and no client ever saw it.
        // Species/Location.cpp gates unit targeting on m_endSetTarget.
        TeamControls sentControls;
        sentControls.m_unitMove = 1;
        sentControls.m_directUnitMove = 1;
        sentControls.m_primaryFireTarget = 1;
        sentControls.m_secondaryFireTarget = 1;
        sentControls.m_primaryFireDirected = 1;
        sentControls.m_secondaryFireDirected = 1;
        sentControls.m_cameraEntityTracking = 1;
        sentControls.m_unitSecondaryMode = 1;
        sentControls.m_endSetTarget = 1;

        Assert::AreEqual(0x01FF, static_cast<int>(sentControls.GetFlags()), L"all nine flags should encode");

        NetworkUpdate sent;
        sent.SetType(NetworkUpdate::UpdateType::Alive);
        sent.SetLastSequenceId(3);
        sent.SetTeamId(1);
        sent.SetTeamControls(sentControls);
        sent.SetWorldPos(sentControls.m_mousePos);
        sent.SetSync(7);

        char datagram[MaxDatagramSize];
        const int length = sent.SerialiseDatagram(datagram, static_cast<int>(sizeof(datagram)));

        NetworkUpdate received(datagram, length);

        Assert::AreEqual(0x01FF, static_cast<int>(received.m_teamControls.GetFlags()));
        Assert::IsTrue(received.m_teamControls.m_endSetTarget == 1, L"m_endSetTarget is the bit that used to be dropped");
      }

      // ------------------------------------------------------------------
      // What a client is allowed to do to the server.
      //
      // These go the other way round from the letter's: every datagram the
      // SERVER receives is parsed by this class, and until network-transport T1
      // it was parsed with no length at all — NeuronServer's listen callback
      // handed over m_data and not m_length, so the switch below read as far as
      // the type it was told to.
      // ------------------------------------------------------------------

      TEST_METHOD(ATruncatedUpdateIsInvalid)
      {
        // A SelectUnit datagram with only part of its payload. The bytes after
        // it in a real receive buffer are the previous datagram.
        NetworkUpdate sent;
        sent.SetType(NetworkUpdate::UpdateType::SelectUnit);
        sent.SetLastSequenceId(1);
        sent.SetTeamId(1);
        sent.SetUnitId(2);
        sent.SetEntityId(3);
        sent.SetBuildingID(4);

        char datagram[MaxDatagramSize];
        const int length = sent.SerialiseDatagram(datagram, static_cast<int>(sizeof(datagram)));
        Assert::AreEqual(4 + 21, length, L"the frame is four bytes in front of the payload");

        const NetworkUpdate received(datagram, 16);

        Assert::IsFalse(received.IsValid());
      }

      TEST_METHOD(AnUpdateTypeThisBuildDoesNotKnowIsInvalid)
      {
        char datagram[12] = {};
        ByteWriter writer(datagram, sizeof(datagram));
        WriteDatagramHeader(writer, DatagramKind::ClientUpdate);
        writer.Write<int>(31337);
        writer.Write<int>(0);

        const NetworkUpdate received(datagram, static_cast<int>(sizeof(datagram)));

        Assert::IsFalse(received.IsValid());
      }

      TEST_METHOD(AZeroLengthDatagramIsAnInvalidUpdate)
      {
        char datagram[1] = {};

        const NetworkUpdate received(datagram, 0);

        Assert::IsFalse(received.IsValid());
      }

      // ------------------------------------------------------------------
      // The frame, which is protocol 2 and does not interoperate with 1.
      //
      // Before it, the first four bytes of ANY datagram arriving on the port
      // were read as an update type. These four tests are what "receivers
      // silently drop anything that does not match" means in practice.
      // ------------------------------------------------------------------

      TEST_METHOD(AFramedUpdateBeginsWithTheMagicVersionAndKind)
      {
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::ClientJoin);
        update.SetLastSequenceId(0x11223344);

        char datagram[MaxDatagramSize];
        const int length = update.SerialiseDatagram(datagram, static_cast<int>(sizeof(datagram)));

        Assert::AreEqual(4 + 12, length);
        Assert::AreEqual(static_cast<char>('S'), datagram[0]);
        Assert::AreEqual(static_cast<char>('P'), datagram[1]);
        Assert::AreEqual(static_cast<char>(2), datagram[2]);
        Assert::AreEqual(static_cast<char>(1), datagram[3]); // ClientUpdate

        // The payload behind the frame is byte-for-byte what it always was —
        // v2 adds a header and changes nothing else.
        int payloadSize = 0;
        char const* payload = update.GetByteStream(&payloadSize);
        Assert::AreEqual(12, payloadSize);
        Assert::AreEqual(0, memcmp(datagram + 4, payload, static_cast<size_t>(payloadSize)));
      }

      TEST_METHOD(ADatagramWithTheWrongMagicIsDropped)
      {
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::ClientJoin);
        update.SetLastSequenceId(1);

        char datagram[MaxDatagramSize];
        const int length = update.SerialiseDatagram(datagram, static_cast<int>(sizeof(datagram)));

        datagram[0] = 'X';
        Assert::IsFalse(NetworkUpdate(datagram, length).IsValid());
      }

      TEST_METHOD(ADatagramFromProtocolOneIsDropped)
      {
        // What a v1 client actually sends: the payload with no frame at all. Its
        // first four bytes are an update type — ClientJoin is 1 — so without the
        // frame this parses cleanly as a join.
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::ClientJoin);
        update.SetLastSequenceId(1);

        int payloadSize = 0;
        char const* payload = update.GetByteStream(&payloadSize);

        char asVersionOne[MaxDatagramSize];
        memcpy(asVersionOne, payload, static_cast<size_t>(payloadSize));

        Assert::IsFalse(NetworkUpdate(asVersionOne, payloadSize).IsValid(), L"v2 does not interoperate with v1, deliberately");
      }

      TEST_METHOD(AServerLetterArrivingAtTheServerIsDropped)
      {
        // The kind byte. A letter's first payload field is a LetterType, and
        // those numbers overlap the UpdateTypes — LetterType::Update is 4 and
        // UpdateType::Alive is 4 — so without the kind, a letter looped back to
        // the server parses as a plausible update.
        char datagram[16] = {};
        ByteWriter writer(datagram, sizeof(datagram));
        WriteDatagramHeader(writer, DatagramKind::ServerLetter);
        writer.Write<int>(4);
        writer.Write<int>(0);
        writer.Write<int>(0);

        Assert::IsFalse(NetworkUpdate(datagram, static_cast<int>(sizeof(datagram))).IsValid());
      }

      TEST_METHOD(PauseStillParsesFromItsHeaderAlone)
      {
        // Pause carries no payload, and it used to work by falling out of the
        // bottom of a switch that did not mention it. A switch where an
        // unlisted type means "not this protocol" would have dropped it
        // silently — nothing else in the suite would have noticed, because
        // nothing else sends one.
        NetworkUpdate sent;
        sent.SetType(NetworkUpdate::UpdateType::Pause);
        sent.SetLastSequenceId(6);

        int payloadSize = 0;
        sent.GetByteStream(&payloadSize);
        Assert::AreEqual(8, payloadSize);

        char datagram[MaxDatagramSize];
        const int length = sent.SerialiseDatagram(datagram, static_cast<int>(sizeof(datagram)));
        const NetworkUpdate received(datagram, length);

        Assert::IsTrue(received.IsValid());
        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::Pause), static_cast<int>(received.m_type));
        Assert::AreEqual(6, received.m_lastSequenceId);
      }

      // THE ENUMERATOR VALUES ARE THE PROTOCOL. NetworkUpdate writes m_type as
      // four bytes and reads it back as four bytes, so a client and a server
      // agree on what an update means only by agreeing on these numbers.
      // Nothing else pins them: the byte-layout tests fix the SIZE of the field,
      // not the meaning of what is in it. Reordering the enum or inserting an
      // enumerator in the middle is a protocol break that would otherwise
      // compile, link and pass every other test.
      //
      // Written before language-hygiene T3 converted the enum to enum class, so
      // that the conversion had to keep these numbers rather than assert its own.
      TEST_METHOD(TheUpdateTypeValuesAreTheProtocol)
      {
        Assert::AreEqual(0, static_cast<int>(NetworkUpdate::UpdateType::Invalid));
        Assert::AreEqual(1, static_cast<int>(NetworkUpdate::UpdateType::ClientJoin));
        Assert::AreEqual(2, static_cast<int>(NetworkUpdate::UpdateType::ClientLeave));
        Assert::AreEqual(3, static_cast<int>(NetworkUpdate::UpdateType::RequestTeam));
        Assert::AreEqual(4, static_cast<int>(NetworkUpdate::UpdateType::Alive));
        Assert::AreEqual(5, static_cast<int>(NetworkUpdate::UpdateType::SelectUnit));
        Assert::AreEqual(6, static_cast<int>(NetworkUpdate::UpdateType::CreateUnit));
        Assert::AreEqual(7, static_cast<int>(NetworkUpdate::UpdateType::AimBuilding));
        Assert::AreEqual(8, static_cast<int>(NetworkUpdate::UpdateType::ToggleLaserFence));
        Assert::AreEqual(9, static_cast<int>(NetworkUpdate::UpdateType::RunProgram));
        Assert::AreEqual(10, static_cast<int>(NetworkUpdate::UpdateType::TargetProgram));
        Assert::AreEqual(11, static_cast<int>(NetworkUpdate::UpdateType::Pause));
        Assert::AreEqual(12, static_cast<int>(NetworkUpdate::UpdateType::Syncronise));
      }
  };
} // namespace NeuronCoreTests
