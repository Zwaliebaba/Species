#include "pch.h"

#include "WorldObject.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
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
  };
} // namespace GameLogicTests
