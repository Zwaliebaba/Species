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
        update.SetType(NetworkUpdate::ClientJoin);
        update.SetLastSequenceId(0x11223344);

        int length = 0;
        char* read = update.GetByteStream(&length);

        const int type = READ_INT(read);
        const int sequenceId = READ_INT(read);

        Assert::AreEqual(static_cast<int>(NetworkUpdate::ClientJoin), type);
        Assert::AreEqual(0x11223344, sequenceId);
        Assert::AreEqual(8, length);
      }

      TEST_METHOD(SelectUnitCarriesTeamUnitEntityAndBuilding)
      {
        // The field set RequestSelectUnit fills. m_unitId, m_entityId and
        // m_buildingId are raw DArray slots — network identity rather than
        // opaque handles — so their order and width are load-bearing.
        NetworkUpdate update;
        update.SetType(NetworkUpdate::SelectUnit);
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

        Assert::AreEqual(static_cast<int>(NetworkUpdate::SelectUnit), type);
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
        update.SetType(NetworkUpdate::CreateUnit);
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

        Assert::AreEqual(static_cast<int>(NetworkUpdate::CreateUnit), type);
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
        update.SetType(NetworkUpdate::Alive);
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

        Assert::AreEqual(static_cast<int>(NetworkUpdate::Alive), type);
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
          NetworkUpdate::ClientJoin, NetworkUpdate::ClientLeave,   NetworkUpdate::RequestTeam, NetworkUpdate::Alive,
          NetworkUpdate::SelectUnit, NetworkUpdate::CreateUnit,    NetworkUpdate::AimBuilding, NetworkUpdate::ToggleLaserFence,
          NetworkUpdate::RunProgram, NetworkUpdate::TargetProgram, NetworkUpdate::Pause,       NetworkUpdate::Syncronise,
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
        sent.SetType(NetworkUpdate::TargetProgram);
        sent.SetLastSequenceId(77);
        sent.SetTeamId(3);
        sent.SetProgram(5);
        sent.SetWorldPos(Vector3(10.25f, -0.5f, 300.0f));

        int length = 0;
        char* stream = sent.GetByteStream(&length);

        NetworkUpdate received(stream);

        Assert::AreEqual(static_cast<int>(NetworkUpdate::TargetProgram), static_cast<int>(received.m_type));
        Assert::AreEqual(77, received.m_lastSequenceId);
        Assert::AreEqual(static_cast<unsigned char>(3), received.m_teamId);
        Assert::AreEqual(static_cast<unsigned char>(5), received.m_program);
        Assert::AreEqual(10.25f, received.GetWorldPos().x);
        Assert::AreEqual(-0.5f, received.GetWorldPos().y);
        Assert::AreEqual(300.0f, received.GetWorldPos().z);
      }
  };
} // namespace NeuronCoreTests
