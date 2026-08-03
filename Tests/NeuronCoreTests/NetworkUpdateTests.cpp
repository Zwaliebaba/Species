#include "pch.h"

#include "ByteStream.h"
#include "NetworkUpdate.h"

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
  // Note the READ_/WRITE_ macros are two statements each, not expressions, so
  // every read below goes through a local. Inlining one into an Assert call does
  // not compile.
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
        _update.SetWorldPos(Vector3(1.0f, 1.0f, 1.0f));
      }

    public:
      TEST_METHOD(TheHeaderIsATypeThenASequenceId)
      {
        // Every type shares this prefix, so two builds that disagree about it
        // disagree about every packet.
        NetworkUpdate update;
        update.SetType(NetworkUpdate::UpdateType::ClientJoin);
        update.SetLastSequenceId(0x11223344);

        int length = 0;
        char* read = update.GetByteStream(&length);

        const int type = READ_INT(read);
        const int sequenceId = READ_INT(read);

        Assert::AreEqual(static_cast<int>(NetworkUpdate::UpdateType::ClientJoin), type);
        Assert::AreEqual(0x11223344, sequenceId);
        Assert::AreEqual(8, length);
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
        char* read = update.GetByteStream(&length);

        const int type = READ_INT(read);
        const int sequenceId = READ_INT(read);
        const unsigned char teamId = READ_UNSIGNED_CHAR(read);
        const int unitId = READ_INT(read);
        const int entityId = READ_INT(read);
        const int buildingId = READ_INT(read);

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
        update.SetWorldPos(Vector3(1.0f, 2.0f, 3.0f));

        int length = 0;
        char* read = update.GetByteStream(&length);

        const int type = READ_INT(read);
        const int sequenceId = READ_INT(read);
        const unsigned char teamId = READ_UNSIGNED_CHAR(read);
        const unsigned char entityType = READ_UNSIGNED_CHAR(read);
        const int numTroops = READ_INT(read);
        const int buildingId = READ_INT(read);
        const float x = READ_FLOAT(read);
        const float y = READ_FLOAT(read);
        const float z = READ_FLOAT(read);

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
        controls.m_mousePos = Vector3(-4.5f, 0.0f, 4.5f);
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
        char* read = update.GetByteStream(&length);

        const int type = READ_INT(read);
        const int sequenceId = READ_INT(read);
        const unsigned char teamId = READ_UNSIGNED_CHAR(read);
        const float x = READ_FLOAT(read);
        const float y = READ_FLOAT(read);
        const float z = READ_FLOAT(read);
        const unsigned short flags = READ_UNSIGNED_SHORT(read);
        const unsigned char sync = READ_UNSIGNED_CHAR(read);

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
        update.SetWorldPos(Vector3(1.0f, 2.0f, 3.0f));
        Assert::AreEqual(1.0f, update.m_teamControls.m_mousePos.x);

        TeamControls empty;
        update.SetTeamControls(empty);
        Assert::AreEqual(0.0f, update.GetWorldPos().x);
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
        sent.SetWorldPos(Vector3(10.25f, -0.5f, 300.0f));

        int length = 0;
        char* stream = sent.GetByteStream(&length);

        NetworkUpdate received(stream);

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

        int length = 0;
        char* stream = sent.GetByteStream(&length);

        NetworkUpdate received(stream);

        Assert::AreEqual(0x01FF, static_cast<int>(received.m_teamControls.GetFlags()));
        Assert::IsTrue(received.m_teamControls.m_endSetTarget == 1, L"m_endSetTarget is the bit that used to be dropped");
      }

      // THE ENUMERATOR VALUES ARE THE PROTOCOL. NetworkUpdate writes m_type with
      // WRITE_INT and reads it back with READ_INT, so a client and a server
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
