#include "pch.h"

#include "EntityGrid.h"
#include "Location.h"
#include "WorldObjectId.h"
#include "WorldPointers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
  namespace
  {
    // EntityGrid sizes itself from the landscape, and Landscape::GetWorldSizeX
    // returns 1e6 when no heightmap has been loaded. A default-constructed
    // Location is therefore a legal, empty world one million units across, and
    // a cell size of 1e5 gives an 11x11 grid per team — small enough to build
    // in a test, large enough that the index arithmetic is still exercised.
    // The game itself uses 8.0f, which against a real landscape is a few
    // hundred cells a side.
    constexpr float CELL_SIZE = 100000.0f;

    // EntityGrid reads g_location in its constructor, so the world has to be
    // installed before one exists and removed after. Location's own
    // constructor allocates nothing that needs a level file; Init() is what
    // loads a map, and nothing here calls it.
    struct EmptyWorld
    {
        Location location;

        EmptyWorld() { g_location = &location; }
        ~EmptyWorld() { g_location = nullptr; }
    };

    bool ALL_TEAMS[NUM_TEAMS] = {true, true, true, true};
  } // namespace

  // The spatial index every entity queries for its neighbours, and the reason
  // layering-inversion T15 moved the simulation cluster into GameLogic: while
  // these classes lived in Species — an executable, with no test project able
  // to link it — none of this could be asserted at all. That matters most for
  // containers-replaced T12, which converts the FastDArray these ids index
  // into; a WorldObjectId's m_index IS a slot number, so any conversion that
  // changes slot allocation changes network identity. These tests are the
  // floor that conversion lands on.
  TEST_CLASS(EntityGridTests)
  {
    public:
      TEST_METHOD(SizesItselfFromTheLandscape)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        // Cell index is world position divided by cell size, truncated.
        Assert::AreEqual(0, grid.GetGridIndexX(0.0f));
        Assert::AreEqual(2, grid.GetGridIndexX(CELL_SIZE * 2.5f));
        Assert::AreEqual(2, grid.GetGridIndexZ(CELL_SIZE * 2.5f));
      }

      TEST_METHOD(AddedObjectIsCounted)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        const WorldObjectId id(1, 0, 7, 42);
        grid.AddObject(id, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f);

        Assert::AreEqual(1, grid.GetNumNeighbours(CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, CELL_SIZE, ALL_TEAMS));
      }

      TEST_METHOD(RemovedObjectIsNotCounted)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        const WorldObjectId id(1, 0, 7, 42);
        grid.AddObject(id, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f);
        grid.RemoveObject(id, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f);

        Assert::AreEqual(0, grid.GetNumNeighbours(CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, CELL_SIZE, ALL_TEAMS));
      }

      TEST_METHOD(ObjectIsFiledUnderItsOwnTeam)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        grid.AddObject(WorldObjectId(1, 0, 7, 42), CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f);

        // The grid keeps a separate cell array per team so that "enemies of
        // team N" is a filter rather than a scan. A query that excludes the
        // object's team must not see it.
        bool teamZeroOnly[NUM_TEAMS] = {true, false, false, false};
        Assert::AreEqual(0, grid.GetNumNeighbours(CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, CELL_SIZE, teamZeroOnly));

        bool teamOneOnly[NUM_TEAMS] = {false, true, false, false};
        Assert::AreEqual(1, grid.GetNumNeighbours(CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, CELL_SIZE, teamOneOnly));
      }

      TEST_METHOD(TeamlessObjectIsIgnored)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        // Team 255 is the invalid-id sentinel. AddObject drops it rather than
        // indexing m_cells out of bounds with it.
        WorldObjectId unset;
        grid.AddObject(unset, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f);

        Assert::AreEqual(0, grid.GetNumNeighbours(CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, CELL_SIZE, ALL_TEAMS));
      }
  };
} // namespace GameLogicTests
