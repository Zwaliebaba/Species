#include "pch.h"

#include "TerrainChunk.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Species;

// The chunk coordinate system is the seam every later large-location task sits
// on, so its arithmetic is pinned before anything is built on it. See
// docs/LARGE_LOCATION.md areas A and B, decisions D1 and D12.
//
// The conversions are integer-exact by construction — 65,536 / 16 = 4,096
// samples, 4,096 / 128 = 32 chunks — so these are equalities, not tolerances.
// If one of them starts needing a tolerance, the chunk grid has stopped tiling
// the world and that is the bug, not the test.

namespace GameLogicTests
{
  TEST_CLASS(TerrainChunkTests)
  {
    public:
      // D12 in one place. If any of these move, the arithmetic below stops
      // being exact and every border computation grows a special case.
      TEST_METHOD(ConstantsTileTheWorldExactly)
      {
        Assert::AreEqual(128, CHUNK_CELLS);
        Assert::AreEqual(129, CHUNK_SAMPLES, L"one more sample than cells: the last is shared with the next chunk");
        Assert::AreEqual(16.0f, LARGE_MAP_CELL_SIZE);
        Assert::AreEqual(2048.0f, CHUNK_WORLD_SIZE);
        Assert::AreEqual(65536, LARGE_MAP_WORLD_SIZE);
        Assert::AreEqual(32, LARGE_MAP_CHUNKS);

        Assert::AreEqual(static_cast<float>(LARGE_MAP_WORLD_SIZE), LARGE_MAP_CHUNKS * CHUNK_WORLD_SIZE,
                         L"the chunk grid tiles the world with no remainder");
        Assert::AreEqual(4096, LARGE_MAP_CHUNKS * CHUNK_CELLS, L"4,096 terrain samples per axis at cell size 16");
      }

      TEST_METHOD(WorldToChunkAtTheOrigin)
      {
        Assert::AreEqual(0, WorldToChunkAxis(0.0f));
        Assert::AreEqual(0, WorldToChunkAxis(1.0f));
        Assert::AreEqual(0, WorldToChunkAxis(CHUNK_WORLD_SIZE - 1.0f), L"the last world unit of chunk 0");
        Assert::AreEqual(1, WorldToChunkAxis(CHUNK_WORLD_SIZE), L"the boundary belongs to the HIGHER chunk");
      }

      TEST_METHOD(WorldToChunkAtEveryChunkBoundary)
      {
        // Both sides of all 31 interior boundaries, so an off-by-one in the
        // division shows up wherever it is rather than only at the origin.
        for (int chunk = 1; chunk < LARGE_MAP_CHUNKS; ++chunk)
        {
          float const boundary = static_cast<float>(chunk) * CHUNK_WORLD_SIZE;
          Assert::AreEqual(chunk - 1, WorldToChunkAxis(boundary - 1.0f), L"just below a boundary is the lower chunk");
          Assert::AreEqual(chunk, WorldToChunkAxis(boundary), L"exactly on a boundary is the higher chunk");
          Assert::AreEqual(chunk, WorldToChunkAxis(boundary + 1.0f), L"just above a boundary is the higher chunk");
        }
      }

      // The far corner of the world. 65,536 is 2^16 and exactly representable,
      // so this is not an approximation: the position one unit inside the map
      // is in the last chunk, and the map's own extent is the first coordinate
      // outside it.
      TEST_METHOD(WorldToChunkAtTheWorldEdge)
      {
        Assert::AreEqual(LARGE_MAP_CHUNKS - 1, WorldToChunkAxis(static_cast<float>(LARGE_MAP_WORLD_SIZE) - 1.0f), L"one unit inside the map");
        Assert::AreEqual(LARGE_MAP_CHUNKS, WorldToChunkAxis(static_cast<float>(LARGE_MAP_WORLD_SIZE)),
                         L"the map extent itself is past the last chunk");

        Assert::IsTrue(ChunkCoord(LARGE_MAP_CHUNKS - 1, LARGE_MAP_CHUNKS - 1).IsInsideLargeMap());
        Assert::IsFalse(ChunkCoord(LARGE_MAP_CHUNKS, 0).IsInsideLargeMap());
        Assert::IsFalse(ChunkCoord(0, LARGE_MAP_CHUNKS).IsInsideLargeMap());
        Assert::IsFalse(ChunkCoord(-1, 0).IsInsideLargeMap());
      }

      // Floor, not truncation. Nothing on the large map is negative today, but
      // the open world's address space is signed and centred on the origin
      // (tasks/_openworld-prompt.md area A), and truncation would put both -1
      // and +1 in chunk 0 — a bug that would first appear a whole milestone
      // later, in code nobody was editing at the time.
      TEST_METHOD(WorldToChunkFloorsRatherThanTruncatesBelowTheOrigin)
      {
        Assert::AreEqual(-1, WorldToChunkAxis(-1.0f), L"one unit below the origin is chunk -1, not chunk 0");
        Assert::AreEqual(-1, WorldToChunkAxis(-CHUNK_WORLD_SIZE), L"exactly one chunk below");
        Assert::AreEqual(-2, WorldToChunkAxis(-CHUNK_WORLD_SIZE - 1.0f));
      }

      TEST_METHOD(ChunkOriginAndLocalOffsetRoundTrip)
      {
        for (int chunk = 0; chunk < LARGE_MAP_CHUNKS; ++chunk)
        {
          float const origin = ChunkOriginAxis(chunk);
          Assert::AreEqual(static_cast<float>(chunk) * CHUNK_WORLD_SIZE, origin);

          // A position inside the chunk decomposes into that chunk plus a local
          // offset, and the two put it back exactly where it started.
          float const world = origin + 37.0f;
          Assert::AreEqual(chunk, WorldToChunkAxis(world));
          Assert::AreEqual(37.0f, WorldToChunkLocalAxis(world), L"local offset is exact, not approximate");
          Assert::AreEqual(world, ChunkOriginAxis(WorldToChunkAxis(world)) + WorldToChunkLocalAxis(world));
        }
      }

      TEST_METHOD(LocalOffsetIsAlwaysInsideTheChunk)
      {
        Assert::AreEqual(0.0f, WorldToChunkLocalAxis(0.0f));
        Assert::AreEqual(0.0f, WorldToChunkLocalAxis(CHUNK_WORLD_SIZE), L"a boundary is offset zero in the higher chunk");
        Assert::AreEqual(CHUNK_WORLD_SIZE - 1.0f, WorldToChunkLocalAxis(CHUNK_WORLD_SIZE - 1.0f));
        Assert::AreEqual(2047.0f, WorldToChunkLocalAxis(2.0f * CHUNK_WORLD_SIZE - 1.0f), L"the last unit of chunk 1");
      }

      TEST_METHOD(WorldToSampleIndexSpansTheChunk)
      {
        Assert::AreEqual(0, WorldToSampleAxis(0.0f));
        Assert::AreEqual(0, WorldToSampleAxis(LARGE_MAP_CELL_SIZE - 1.0f), L"still inside the first cell");
        Assert::AreEqual(1, WorldToSampleAxis(LARGE_MAP_CELL_SIZE));
        Assert::AreEqual(CHUNK_CELLS - 1, WorldToSampleAxis(CHUNK_WORLD_SIZE - LARGE_MAP_CELL_SIZE), L"the last cell of the chunk");

        // One past the last cell is sample 0 of the next chunk, not sample 128
        // of this one — which is the whole point of resolving the chunk first.
        Assert::AreEqual(0, WorldToSampleAxis(CHUNK_WORLD_SIZE));
      }

      // THE SHARED-BORDER RULE. Sample 128 of a chunk occupies the same world
      // position as sample 0 of the next chunk along. Generation must produce
      // the same height for both, which is what makes borders continuous rather
      // than merely close. Pinned here as a property of the coordinate system;
      // that generation actually honours it is large-location T6's to prove.
      TEST_METHOD(TheLastSampleOfAChunkIsTheFirstSampleOfTheNext)
      {
        Assert::IsTrue(ChunkSampleIsShared(CHUNK_SAMPLES - 1), L"sample 128 is the shared one");
        Assert::IsFalse(ChunkSampleIsShared(0));
        Assert::IsFalse(ChunkSampleIsShared(CHUNK_CELLS - 1));

        TerrainChunk const left(ChunkCoord(0, 0));
        TerrainChunk const right(ChunkCoord(1, 0));

        // The world position of the shared column, computed from each side.
        float const fromLeft = left.GetWorldMinX() + static_cast<float>(CHUNK_SAMPLES - 1) * LARGE_MAP_CELL_SIZE;
        float const fromRight = right.GetWorldMinX();
        Assert::AreEqual(fromRight, fromLeft, L"left's sample 128 and right's sample 0 are the same world position");
        Assert::AreEqual(left.GetWorldMaxX(), fromRight, L"and that position is left's exclusive upper bound");
      }

      TEST_METHOD(ChunkBoundsCoverTheChunkAndMeetTheNeighbour)
      {
        TerrainChunk const chunk(ChunkCoord(3, 5));

        Assert::AreEqual(3.0f * CHUNK_WORLD_SIZE, chunk.GetWorldMinX());
        Assert::AreEqual(5.0f * CHUNK_WORLD_SIZE, chunk.GetWorldMinZ());
        Assert::AreEqual(4.0f * CHUNK_WORLD_SIZE, chunk.GetWorldMaxX(), L"the maximum is the next chunk's origin");
        Assert::AreEqual(6.0f * CHUNK_WORLD_SIZE, chunk.GetWorldMaxZ());

        // Every world position inside the bounds resolves back to this chunk.
        Assert::IsTrue(WorldToChunk(chunk.GetWorldMinX(), chunk.GetWorldMinZ()) == chunk.GetCoord());
        Assert::IsTrue(WorldToChunk(chunk.GetWorldMaxX() - 1.0f, chunk.GetWorldMaxZ() - 1.0f) == chunk.GetCoord());
        Assert::IsFalse(WorldToChunk(chunk.GetWorldMaxX(), chunk.GetWorldMinZ()) == chunk.GetCoord());
      }

      TEST_METHOD(ChunkCoordEqualityOrderingAndIndex)
      {
        Assert::IsTrue(ChunkCoord(2, 3) == ChunkCoord(2, 3));
        Assert::IsTrue(ChunkCoord(2, 3) != ChunkCoord(3, 2));

        // Row-major, z-then-x, so the ordering and the index agree. Region
        // activation breaks eviction ties with this order precisely so two
        // clients cannot make different choices from the same state.
        Assert::IsTrue(ChunkCoord(5, 1) < ChunkCoord(0, 2), L"a lower z sorts first regardless of x");
        Assert::IsTrue(ChunkCoord(0, 1) < ChunkCoord(1, 1), L"within a row, lower x sorts first");
        Assert::IsFalse(ChunkCoord(1, 1) < ChunkCoord(1, 1), L"a strict order: equal is not less");

        Assert::AreEqual(0, ChunkCoord(0, 0).LargeMapIndex());
        Assert::AreEqual(31, ChunkCoord(31, 0).LargeMapIndex());
        Assert::AreEqual(32, ChunkCoord(0, 1).LargeMapIndex());
        Assert::AreEqual(1023, ChunkCoord(31, 31).LargeMapIndex(), L"1,024 chunks, last index 1,023");
      }

      // The storage is a SurfaceMap2D positioned at the chunk's world origin,
      // so a world-space query needs no translation at the call site — which is
      // what lets chunked storage sit behind Landscape's existing query API.
      TEST_METHOD(HeightMapIsSampleSizedAndWorldPositioned)
      {
        TerrainChunk chunk(ChunkCoord(2, 1));

        Assert::AreEqual(CHUNK_SAMPLES, static_cast<int>(chunk.GetHeightMap().GetNumColumns()));
        Assert::AreEqual(CHUNK_SAMPLES, static_cast<int>(chunk.GetHeightMap().GetNumRows()));
        Assert::AreEqual(CHUNK_SAMPLES, static_cast<int>(chunk.GetNormalMap().GetNumColumns()));
        Assert::AreEqual(CHUNK_SAMPLES, static_cast<int>(chunk.GetNormalMap().GetNumRows()));

        Assert::AreEqual(chunk.GetWorldMinX(), chunk.GetHeightMap().m_x0, L"positioned at the chunk origin");
        Assert::AreEqual(chunk.GetWorldMinZ(), chunk.GetHeightMap().m_y0);
        Assert::AreEqual(LARGE_MAP_CELL_SIZE, chunk.GetHeightMap().m_cellSizeX);

        // A sample written by chunk-local index reads back through a WORLD
        // coordinate query, with no translation in between.
        chunk.PutHeightSample(4, 6, 42.0f);
        Assert::AreEqual(42.0f, chunk.GetHeightSample(4, 6));

        float const worldX = chunk.GetWorldMinX() + 4.0f * LARGE_MAP_CELL_SIZE;
        float const worldZ = chunk.GetWorldMinZ() + 6.0f * LARGE_MAP_CELL_SIZE;
        Assert::AreEqual(42.0f, chunk.GetHeightMap().GetValue(worldX, worldZ), L"world-space query lands on the sample");
      }

      TEST_METHOD(HeightBoundsCoverEverySample)
      {
        TerrainChunk chunk(ChunkCoord(0, 0));

        for (int z = 0; z < CHUNK_SAMPLES; ++z)
        {
          for (int x = 0; x < CHUNK_SAMPLES; ++x)
          {
            chunk.PutHeightSample(x, z, static_cast<float>(x - z));
          }
        }
        chunk.PutHeightSample(9, 9, -500.0f);
        chunk.PutHeightSample(11, 11, 900.0f);

        chunk.RecalculateHeightBounds();
        Assert::AreEqual(-500.0f, chunk.GetMinHeight());
        Assert::AreEqual(900.0f, chunk.GetMaxHeight());

        // The envelope is the promise a conservative query relies on: no sample
        // in the chunk lies outside it.
        for (int z = 0; z < CHUNK_SAMPLES; ++z)
        {
          for (int x = 0; x < CHUNK_SAMPLES; ++x)
          {
            float const height = chunk.GetHeightSample(x, z);
            Assert::IsTrue(height >= chunk.GetMinHeight() && height <= chunk.GetMaxHeight(), L"every sample is inside the envelope");
          }
        }
      }
  };
} // namespace GameLogicTests
