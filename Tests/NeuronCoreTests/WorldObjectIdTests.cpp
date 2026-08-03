#include "pch.h"

#include "ByteStream.h"
#include "WorldObjectId.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // WorldObjectId is network identity. The whole struct goes onto the wire
  // through WRITE_WORLDOBJECTID, and m_index is a raw DArray slot — which is why
  // replacing a DArray with a std::vector repoints every reference in the world
  // (see CODING_STANDARDS.md, Determinism). Its comparison and validity rules
  // are therefore protocol, not convenience.
  TEST_CLASS(WorldObjectIdTests)
  {
    public:
      TEST_METHOD(DefaultConstructedIsInvalid)
      {
        const WorldObjectId id;
        Assert::IsFalse(id.IsValid());
      }

      TEST_METHOD(SetInvalidUndoesSet)
      {
        WorldObjectId id(0, 1, 2, 3);
        Assert::IsTrue(id.IsValid());

        id.SetInvalid();
        Assert::IsFalse(id.IsValid());
      }

      TEST_METHOD(TeamZeroUnitZeroIndexZeroIsValid)
      {
        // The invalid sentinel is team 255 / unit -1 / index -1, and IsValid
        // ORs the three. An all-zero id is a real object on team 0 and must not
        // be mistaken for an unset one.
        const WorldObjectId id(0, 0, 0, 0);
        Assert::IsTrue(id.IsValid());
      }

      TEST_METHOD(EqualityComparesEveryField)
      {
        const WorldObjectId id(1, 2, 3, 4);

        Assert::IsTrue(id == WorldObjectId(1, 2, 3, 4));
        Assert::IsTrue(id != WorldObjectId(9, 2, 3, 4));
        Assert::IsTrue(id != WorldObjectId(1, 9, 3, 4));
        Assert::IsTrue(id != WorldObjectId(1, 2, 9, 4));

        // The unique id is part of identity too: a slot that was freed and
        // reused has the same team, unit and index as the object that used to
        // live there, and only the unique id separates them.
        Assert::IsTrue(id != WorldObjectId(1, 2, 3, 9));
      }

      TEST_METHOD(GenerateUniqueIdNeverRepeats)
      {
        WorldObjectId first;
        WorldObjectId second;

        first.GenerateUniqueId();
        second.GenerateUniqueId();

        Assert::AreNotEqual(first.GetUniqueId(), second.GetUniqueId());
      }

      TEST_METHOD(AssignmentCopiesEveryField)
      {
        const WorldObjectId source(7, 8, 9, 10);
        WorldObjectId target;

        target = source;

        Assert::IsTrue(target == source);
      }

      TEST_METHOD(TheWireLayoutIsSixteenBytesWithTheTeamByteFirst)
      {
        // WRITE_WORLDOBJECTID raw-copies the struct, so its size, its field
        // offsets and even the three padding bytes after m_teamId are wire
        // format. MSVC lays it out as the team byte at offset 0, three bytes
        // of padding, then the three ints. If this fails, the protocol
        // changed: adding a field, reordering, or packing all desync an old
        // build against a new one.
        Assert::AreEqual(16, static_cast<int>(sizeof(WorldObjectId)));

        alignas(WorldObjectId) char buffer[sizeof(WorldObjectId) * 2] = {};
        char* stream = buffer;
        const WorldObjectId id(3, 7, 11, 13);
        WRITE_WORLDOBJECTID(stream, id);

        Assert::AreEqual(16, static_cast<int>(stream - buffer));
        Assert::AreEqual(static_cast<char>(3), buffer[0]);

        int unitId = 0;
        int index = 0;
        int uniqueId = 0;
        memcpy(&unitId, buffer + 4, sizeof(int));
        memcpy(&index, buffer + 8, sizeof(int));
        memcpy(&uniqueId, buffer + 12, sizeof(int));
        Assert::AreEqual(7, unitId);
        Assert::AreEqual(11, index);
        Assert::AreEqual(13, uniqueId);

        char* readStream = buffer;
        const WorldObjectId received = READ_WORLDOBJECTID(readStream);
        Assert::IsTrue(received == id);
      }
  };
} // namespace NeuronCoreTests
