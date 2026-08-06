#pragma once

#include <cmath>

#include "2dSurfaceMap.h"
#include "NeuronMath.h"


// ****************************************************************************
//  Chunked terrain: the coordinate system and the per-chunk storage record.
//
//  This is the seam the whole 65,536 x 65,536 Location rests on. See
//  docs/LARGE_LOCATION.md areas A and B; the decisions it implements are D12
//  (cell size 16, 32x32 chunks of 128x128 cells) and D13 (SurfaceMap2D is the
//  per-chunk container, so the 16-bit index type in Array2D is never widened -
//  a chunk is 129 samples on a side and cannot come near the ceiling).
//
//  WHY THE CONVERSIONS LIVE IN EXACTLY ONE PLACE. The open world addresses
//  space with a pair of int64s (tasks/_openworld-prompt.md area A). This design
//  is its first milestone and deliberately keeps world positions in float
//  XMFLOAT3, which is exact to 2^24 and so covers 65,536 with room to spare.
//  When the address space widens, ChunkCoord is what changes: world -> chunk +
//  local offset is the same shape at either width, and nothing outside this
//  header does that arithmetic by hand.
// ****************************************************************************


namespace Species
{
  // ------------------------------------------------------------------------
  // The constants. One place, per D12.
  //
  // These are chunk-exact by construction: 65,536 / 16 = 4,096 samples per
  // axis, and 4,096 / 128 = 32 chunks exactly, so no chunk is ragged and no
  // border computation needs a special case. That exactness is the whole reason
  // cell size 16 was chosen over the LandscapeDef default of 12, which gives
  // 5,461.33 samples and divides evenly by nothing.
  // ------------------------------------------------------------------------

  // Terrain cells along one edge of a chunk.
  constexpr int CHUNK_CELLS = 128;

  // Height samples along one edge of a chunk. One more than the cell count:
  // the last row and column are SHARED with the next chunk along, which is what
  // makes a chunk's terrain meet its neighbour's exactly rather than merely
  // closely. See ChunkSampleIsShared below.
  constexpr int CHUNK_SAMPLES = CHUNK_CELLS + 1;

  // World units per terrain cell on the large map (D12).
  constexpr float LARGE_MAP_CELL_SIZE = 16.0f;

  // World units along one edge of a chunk: 128 * 16 = 2,048.
  constexpr float CHUNK_WORLD_SIZE = CHUNK_CELLS * LARGE_MAP_CELL_SIZE;

  // World units along one edge of the large map: 32 * 2,048 = 65,536 (D1).
  constexpr int LARGE_MAP_WORLD_SIZE = 65536;

  // Chunks along one edge of the large map: 32.
  constexpr int LARGE_MAP_CHUNKS = LARGE_MAP_WORLD_SIZE / static_cast<int>(CHUNK_WORLD_SIZE);

  static_assert(LARGE_MAP_CHUNKS * static_cast<int>(CHUNK_WORLD_SIZE) == LARGE_MAP_WORLD_SIZE,
                "the chunk grid must tile the world exactly - a ragged edge chunk puts a special case in every border computation");
  static_assert(LARGE_MAP_CHUNKS == 32, "D12 fixes the large map at 32x32 chunks");
  static_assert(CHUNK_SAMPLES < 65535, "a chunk's SurfaceMap2D must sit far under Array2D's 16-bit index ceiling");


  // ------------------------------------------------------------------------
  // Class ChunkCoord
  //
  // Which chunk, not where in it. Cheap, ordered, and hashable, because the
  // residency policy and the population ledger both key on it.
  // ------------------------------------------------------------------------

  class ChunkCoord
  {
    public:
      int m_x{0};
      int m_z{0};

      constexpr ChunkCoord() = default;
      constexpr ChunkCoord(int _x, int _z)
        : m_x(_x),
          m_z(_z)
      {
      }

      constexpr bool operator==(ChunkCoord const& _other) const { return m_x == _other.m_x && m_z == _other.m_z; }
      constexpr bool operator!=(ChunkCoord const& _other) const { return !(*this == _other); }

      // A total order, so anything that has to break a tie between two chunks
      // can do it the same way on every client. Region activation relies on
      // this: eviction ties are broken in chunk-coordinate order precisely so
      // that two clients never evict different regions from the same state.
      constexpr bool operator<(ChunkCoord const& _other) const { return m_z != _other.m_z ? m_z < _other.m_z : m_x < _other.m_x; }

      // Row-major index within the large map's 32x32 grid. Only meaningful for
      // a coordinate that IsInsideLargeMap.
      constexpr int LargeMapIndex() const { return m_z * LARGE_MAP_CHUNKS + m_x; }

      constexpr bool IsInsideLargeMap() const { return m_x >= 0 && m_x < LARGE_MAP_CHUNKS && m_z >= 0 && m_z < LARGE_MAP_CHUNKS; }
  };


  // ------------------------------------------------------------------------
  // World <-> chunk conversions. THE ONLY PLACE THIS ARITHMETIC IS WRITTEN.
  // ------------------------------------------------------------------------

  // Floor division, not truncation: a world coordinate of -1 belongs to chunk
  // -1, not chunk 0. Nothing on the large map is negative today, but the open
  // world's address space is signed and centred on the origin, so getting this
  // wrong here would be a bug that only appears at that milestone. std::floor
  // is exact for every value a float can hold at this magnitude.
  inline int WorldToChunkAxis(float _world) { return static_cast<int>(std::floor(_world / CHUNK_WORLD_SIZE)); }

  inline ChunkCoord WorldToChunk(float _worldX, float _worldZ) { return ChunkCoord(WorldToChunkAxis(_worldX), WorldToChunkAxis(_worldZ)); }

  // The world position of a chunk's origin - its lowest-coordinate corner, and
  // the position of its sample (0, 0).
  inline float ChunkOriginAxis(int _chunkAxis) { return static_cast<float>(_chunkAxis) * CHUNK_WORLD_SIZE; }

  // Where a world position sits INSIDE its chunk, in world units, always in
  // [0, CHUNK_WORLD_SIZE). Simulation and rendering work in this frame; only
  // storage and identity work in chunk coordinates.
  inline float WorldToChunkLocalAxis(float _world) { return _world - ChunkOriginAxis(WorldToChunkAxis(_world)); }

  // A chunk's sample index (0 .. CHUNK_SAMPLES-1) for a world coordinate.
  // Truncating, so it names the sample at or below the position.
  inline int WorldToSampleAxis(float _world) { return static_cast<int>(WorldToChunkLocalAxis(_world) / LARGE_MAP_CELL_SIZE); }

  // True for the shared row or column - the last sample on an axis, which is
  // also sample 0 of the next chunk along. Generation must produce the same
  // value for both, which is what makes chunk borders continuous rather than
  // merely close.
  constexpr bool ChunkSampleIsShared(int _sampleIndex) { return _sampleIndex == CHUNK_SAMPLES - 1; }


  // ------------------------------------------------------------------------
  // Class TerrainChunk
  //
  // One chunk's generated terrain. Heights and normals only - population lives
  // in the ledger and buildings in the obstruction grid, both keyed by
  // ChunkCoord rather than held here.
  //
  // The blocks are SurfaceMap2D so that every existing query path keeps
  // working unchanged: the height map is sampled through GetValue, which
  // interpolates, and the normal map through its XMFLOAT3 specialisation. The
  // maps are constructed at the chunk's world origin, so a world-space query
  // needs no translation at the call site.
  // ------------------------------------------------------------------------

  class TerrainChunk
  {
    public:
      explicit TerrainChunk(ChunkCoord const& _coord, float _outsideHeight = 0.0f);

      ChunkCoord const& GetCoord() const { return m_coord; }

      // World-space bounds of this chunk. The maximum is exclusive: it is the
      // next chunk's origin, and the sample there is the shared one.
      float GetWorldMinX() const { return ChunkOriginAxis(m_coord.m_x); }
      float GetWorldMinZ() const { return ChunkOriginAxis(m_coord.m_z); }
      float GetWorldMaxX() const { return GetWorldMinX() + CHUNK_WORLD_SIZE; }
      float GetWorldMaxZ() const { return GetWorldMinZ() + CHUNK_WORLD_SIZE; }

      Neuron::SurfaceMap2D<float>& GetHeightMap() { return m_heightMap; }
      Neuron::SurfaceMap2D<float> const& GetHeightMap() const { return m_heightMap; }

      Neuron::SurfaceMap2D<DirectX::XMFLOAT3>& GetNormalMap() { return m_normalMap; }
      Neuron::SurfaceMap2D<DirectX::XMFLOAT3> const& GetNormalMap() const { return m_normalMap; }

      // Direct sample access by chunk-local index, for generation and for the
      // border checks. Bounds-checked by Array2D underneath.
      float GetHeightSample(int _x, int _z) const;
      void PutHeightSample(int _x, int _z, float _height);

      // The always-resident envelope (docs/LARGE_LOCATION.md area B): the
      // bounds a conservative ray or line-of-sight test resolves against before
      // forcing this chunk to generate. Maintained by whoever fills the
      // samples; RecalculateHeightBounds derives them from what is stored.
      float GetMinHeight() const { return m_minHeight; }
      float GetMaxHeight() const { return m_maxHeight; }
      void RecalculateHeightBounds();

    private:
      ChunkCoord m_coord;

      // CHUNK_SAMPLES on a side, positioned at the chunk's world origin, so
      // GetValue takes world coordinates directly.
      Neuron::SurfaceMap2D<float> m_heightMap;
      Neuron::SurfaceMap2D<DirectX::XMFLOAT3> m_normalMap;

      float m_minHeight{0.0f};
      float m_maxHeight{0.0f};
  };
} // namespace Species
