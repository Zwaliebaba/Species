#include "pch.h"

#include "EntityGrid.h"
#include "Location.h"
#include "Team.h"
#include "WorldObjectId.h"
#include "WorldPointers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// GameLogic is in namespace Species since namespace-migration T4, and unlike
// namespace Neuron it has no tree-wide using-directive to reach it by. A test
// source is the right place for one: it is a .cpp, so nothing includes it.
using namespace Species;

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

      // ----------------------------------------------------------------
      // CHARACTERISATION FOR large-location T11, which replaces the dense
      // per-team cell array with a hash keyed on (cellX, cellZ, team). The
      // storage changes; none of the behaviour below may.
      //
      // WHAT THESE CANNOT REACH, AND WHY IT MATTERS. The property T11 is most
      // at risk of breaking is the ORDER GetNeighbours returns ids in — it
      // feeds target selection, which feeds positions, which feed the sync
      // value. GetNeighbours cannot be called from a test DLL: it resolves
      // every candidate through Location::GetEntity (EntityGrid.cpp:475),
      // which dereferences m_teams (Location.cpp:448), and m_teams is null on
      // a default-constructed Location. Building a Location with initialised
      // teams and units is the untested-entity-behaviour gap AGENTS.md already
      // records; it is not opened here.
      //
      // So the ordering proof is the Debug shadow-mode dual verification T11
      // carries, which runs inside the real game where GetEntity works. That
      // is not a nice-to-have on this task — it is the ONLY thing that can
      // check the property. What the tests below pin instead is everything
      // that decides which cells are visited at all, which is the input the
      // ordering is computed from.
      // ----------------------------------------------------------------

      // Cell membership is decided by truncating division, so a negative world
      // coordinate lands in cell 0 rather than a negative cell. The sparse
      // grid must reproduce this rather than "fixing" it: an object at a
      // negative position is currently findable from cell 0, and quietly
      // moving it to a cell nobody queries would lose it.
      TEST_METHOD(NegativeCoordinatesTruncateTowardsCellZero)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        Assert::AreEqual(0, grid.GetGridIndexX(-1.0f), L"truncation, not floor");
        Assert::AreEqual(0, grid.GetGridIndexX(-CELL_SIZE * 0.5f));
        Assert::AreEqual(0, grid.GetGridIndexZ(-CELL_SIZE * 0.5f));
        Assert::AreEqual(0, grid.GetGridIndexX(0.0f), L"and zero is the same cell");
      }

      TEST_METHOD(CellIndexIsExactAtCellBoundaries)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        Assert::AreEqual(0, grid.GetGridIndexX(CELL_SIZE - 1.0f), L"just inside cell 0");
        Assert::AreEqual(1, grid.GetGridIndexX(CELL_SIZE), L"a boundary belongs to the higher cell");
        Assert::AreEqual(1, grid.GetGridIndexX(CELL_SIZE + 1.0f));
        Assert::AreEqual(3, grid.GetGridIndexZ(CELL_SIZE * 3.0f));
      }

      // An object with a radius is filed in EVERY cell its box touches, so a
      // query that reaches any one of them finds it. Note the halving: the
      // radius is applied as _radius / 2 on each side (EntityGrid.cpp:275-278),
      // so the parameter behaves as a diameter. Pinned as-is — T11 changes
      // storage, not this arithmetic.
      TEST_METHOD(ObjectWithRadiusOccupiesEveryCellItsBoxTouches)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        // Centred on the corner where cells (1,1), (2,1), (1,2) and (2,2) meet,
        // with a radius reaching CELL_SIZE/2 each way: all four cells.
        float const centre = CELL_SIZE * 1.5f;
        const WorldObjectId id(1, 0, 7, 42);
        grid.AddObject(id, centre, centre, CELL_SIZE);

        // A query whose range covers only ONE of the four still finds it, from
        // whichever cell it looks in.
        bool const inCell11 = grid.AreNeighboursPresent(CELL_SIZE * 1.1f, CELL_SIZE * 1.1f, 1.0f, ALL_TEAMS);
        bool const inCell22 = grid.AreNeighboursPresent(CELL_SIZE * 2.9f, CELL_SIZE * 2.9f, 1.0f, ALL_TEAMS);
        Assert::IsTrue(inCell11, L"found from the low corner cell");
        Assert::IsTrue(inCell22, L"found from the high corner cell");
      }

      // The dedup in the query path: an object occupying four cells is counted
      // ONCE by a query that spans all four. The sparse grid keeps this by
      // keeping the same walk and the same already-added scan.
      TEST_METHOD(MultiCellObjectIsCountedOnce)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        float const centre = CELL_SIZE * 1.5f;
        grid.AddObject(WorldObjectId(1, 0, 7, 42), centre, centre, CELL_SIZE);

        Assert::AreEqual(1, grid.GetNumNeighbours(centre, centre, CELL_SIZE * 2.0f, ALL_TEAMS), L"four cells, one object, one count");
      }

      TEST_METHOD(RemoveClearsEveryCellTheObjectOccupied)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        float const centre = CELL_SIZE * 1.5f;
        const WorldObjectId id(1, 0, 7, 42);
        grid.AddObject(id, centre, centre, CELL_SIZE);
        grid.RemoveObject(id, centre, centre, CELL_SIZE);

        // Every cell it was in, checked individually rather than in one sweep,
        // so a removal that missed a cell is not hidden by the dedup.
        Assert::IsFalse(grid.AreNeighboursPresent(CELL_SIZE * 1.1f, CELL_SIZE * 1.1f, 1.0f, ALL_TEAMS));
        Assert::IsFalse(grid.AreNeighboursPresent(CELL_SIZE * 2.9f, CELL_SIZE * 1.1f, 1.0f, ALL_TEAMS));
        Assert::IsFalse(grid.AreNeighboursPresent(CELL_SIZE * 1.1f, CELL_SIZE * 2.9f, 1.0f, ALL_TEAMS));
        Assert::IsFalse(grid.AreNeighboursPresent(CELL_SIZE * 2.9f, CELL_SIZE * 2.9f, 1.0f, ALL_TEAMS));
        Assert::AreEqual(0, grid.GetNumNeighbours(centre, centre, CELL_SIZE * 2.0f, ALL_TEAMS));
      }

      // Several objects in one cell, so the conversion cannot quietly turn a
      // per-cell list into a per-cell single slot.
      TEST_METHOD(OneCellHoldsManyObjects)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        float const centre = CELL_SIZE * 1.5f;
        for (int i = 0; i < 8; ++i)
        {
          grid.AddObject(WorldObjectId(1, 0, i, 100 + i), centre, centre, 0.0f);
        }

        Assert::AreEqual(8, grid.GetNumNeighbours(centre, centre, CELL_SIZE, ALL_TEAMS));

        // And removing one leaves the rest, exercising the cell's free list.
        grid.RemoveObject(WorldObjectId(1, 0, 3, 103), centre, centre, 0.0f);
        Assert::AreEqual(7, grid.GetNumNeighbours(centre, centre, CELL_SIZE, ALL_TEAMS));
      }

      // UpdateObject is the hot path — every moving entity calls it every tick
      // — and it is the one that must file and unfile across a cell boundary.
      TEST_METHOD(UpdateMovesAnObjectBetweenCells)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        const WorldObjectId id(1, 0, 7, 42);
        float const from = CELL_SIZE * 1.5f;
        float const to = CELL_SIZE * 4.5f;

        grid.AddObject(id, from, from, 0.0f);
        grid.UpdateObject(id, from, from, to, to, 0.0f);

        Assert::IsFalse(grid.AreNeighboursPresent(from, from, 1.0f, ALL_TEAMS), L"gone from the old cell");
        Assert::IsTrue(grid.AreNeighboursPresent(to, to, 1.0f, ALL_TEAMS), L"present in the new one");
        Assert::AreEqual(1, grid.GetNumNeighbours(to, to, CELL_SIZE, ALL_TEAMS), L"and only once");
      }

      // A query whose rectangle runs off the grid is clamped rather than
      // reading out of bounds. The sparse grid has no bounds to clamp against
      // in the same way — a missing cell is simply empty — so this pins the
      // OBSERVABLE behaviour the conversion must preserve.
      TEST_METHOD(QueriesOffTheEdgeOfTheGridAreClampedNotWrapped)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        float const nearOrigin = CELL_SIZE * 0.5f;
        grid.AddObject(WorldObjectId(1, 0, 7, 42), nearOrigin, nearOrigin, 0.0f);

        // A huge range from the origin: the rectangle clamps to the grid and
        // finds the one object, rather than wrapping around to it twice.
        Assert::AreEqual(1, grid.GetNumNeighbours(nearOrigin, nearOrigin, CELL_SIZE * 100.0f, ALL_TEAMS));

        // A query centred far outside the world finds nothing, and does not
        // alias back into cell zero the way SurfaceMap2D::GetValue does.
        Assert::AreEqual(0, grid.GetNumNeighbours(CELL_SIZE * 50.0f, CELL_SIZE * 50.0f, CELL_SIZE, ALL_TEAMS));
      }

      // The counting and presence variants must agree with each other, since
      // the conversion touches the walk they share.
      TEST_METHOD(CountAndPresenceVariantsAgree)
      {
        EmptyWorld world;
        EntityGrid grid(CELL_SIZE, CELL_SIZE);

        float const centre = CELL_SIZE * 1.5f;
        grid.AddObject(WorldObjectId(1, 0, 7, 42), centre, centre, 0.0f);
        grid.AddObject(WorldObjectId(1, 0, 8, 43), centre, centre, 0.0f);

        Assert::AreEqual(2, grid.GetNumNeighbours(centre, centre, CELL_SIZE, ALL_TEAMS));
        Assert::IsTrue(grid.AreNeighboursPresent(centre, centre, CELL_SIZE, ALL_TEAMS));

        bool teamZeroOnly[NUM_TEAMS] = {true, false, false, false};
        Assert::AreEqual(0, grid.GetNumNeighbours(centre, centre, CELL_SIZE, teamZeroOnly));
        Assert::IsFalse(grid.AreNeighboursPresent(centre, centre, CELL_SIZE, teamZeroOnly), L"presence agrees with the count on an excluded team");
      }
  };
} // namespace GameLogicTests

namespace GameLogicTests
{
  // The seam the layers below GameLogic reach the world through
  // (tasks/layering-inversion.yaml T16). Location installs itself in its
  // constructor and removes itself in its destructor, so the pointer cannot go
  // stale the way one maintained by assignment sites would.
  //
  // The destructor's `only if it is me` guard is what these assert. Main.cpp
  // destroys the old world before building the new one, which an unconditional
  // clear would also survive — but a caller that built the replacement first
  // would have the old world's destructor null a pointer to the live one, and
  // the four call sites on the other side would silently stop working.
  TEST_CLASS(LocationAccessSeamTests)
  {
    public:
      TEST_METHOD(AWorldInstallsAndUninstallsItself)
      {
        Assert::IsNull(g_locationAccess);
        {
          Location location;
          Assert::IsTrue(g_locationAccess == &location);
        }
        Assert::IsNull(g_locationAccess);
      }

      TEST_METHOD(DestroyingTheOldWorldAfterTheNewOneKeepsTheSeam)
      {
        auto* first = new Location();
        Assert::IsTrue(g_locationAccess == first);

        auto* second = new Location(); // replacement built before the old one dies
        Assert::IsTrue(g_locationAccess == second);

        delete first; // must not clear a pointer to the live world
        Assert::IsTrue(g_locationAccess == second);

        delete second;
        Assert::IsNull(g_locationAccess);
      }

      TEST_METHOD(AnUnloadedWorldReportsZeroGroundHeight)
      {
        // SoundInstance asks this for every ground-linked sound. Before the
        // seam it read m_landscape.m_heightMap->GetValue directly, which is a
        // null dereference on a Location that has not loaded a level.
        Location location;
        Assert::AreEqual(0.0f, location.GroundHeight(100.0f, 100.0f), 0.0001f);
      }

      TEST_METHOD(AnUnknownIdHasNoSoundSource)
      {
        Location location;

        // Teams have to exist before an id can be looked up at all.
        // Location::GetEntity reads m_teams[teamId].m_teamType with no null
        // check, and m_teams stays null until Init() loads a level — so a
        // default-constructed Location answers GroundHeight (which this task
        // guarded) but crashes on any id query. Team's constructor marks each
        // one TeamTypeUnused, which is what makes the lookup below return
        // cleanly rather than walking an empty unit list.
        location.m_teams = new Team[NUM_TEAMS];

        DirectX::XMFLOAT3 pos(1.0f, 2.0f, 3.0f);
        DirectX::XMFLOAT3 vel(4.0f, 5.0f, 6.0f);
        Assert::IsFalse(location.GetSoundSource(WorldObjectId(1, 0, 7, 42), &pos, &vel));

        // Documented contract: the outputs are untouched when it returns false.
        Assert::AreEqual(1.0f, pos.x, 0.0001f);
        Assert::AreEqual(4.0f, vel.x, 0.0001f);
      }
  };
} // namespace GameLogicTests
