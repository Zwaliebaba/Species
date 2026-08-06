#include "pch.h"

#include <float.h>

#include "TerrainChunk.h"


namespace Species
{
  // The SurfaceMap2D constructor sizes itself as ceilf(extent / cellSize), so
  // the extent handed to it covers CHUNK_SAMPLES cells rather than CHUNK_CELLS:
  // 129 * 16 = 2,064, which ceils to exactly 129 columns and rows. The extra
  // sample over the chunk's own 2,048 world units is the shared border row and
  // column - sample 128 on each axis is sample 0 of the next chunk along.
  //
  // The maps are positioned at the chunk's world origin (x0, y0), so a caller
  // hands GetValue a WORLD coordinate and no translation happens at the call
  // site. That is what lets the chunked storage sit behind Landscape's existing
  // query API without every caller learning about chunks.
  //
  // On SurfaceMap2D's wrap-to-zero at the far edge (see
  // Tests/NeuronClientTests/SurfaceMap2DTests.cpp, and landscape-index-safety
  // T4 which fixes it): a query anywhere inside this chunk's own bounds cannot
  // reach it. At the far border the local coordinate is exactly 2,048, so the
  // upper sample index is 128 - the last valid one - and the wrap needs 129.
  // A query PAST GetWorldMaxX would reach it, which is why callers resolve the
  // chunk first and query second, never the other way round.
  namespace
  {
    constexpr float CHUNK_MAP_EXTENT = static_cast<float>(CHUNK_SAMPLES) * LARGE_MAP_CELL_SIZE;
  } // namespace


  TerrainChunk::TerrainChunk(ChunkCoord const& _coord, float _outsideHeight)
    : m_coord(_coord),
      m_heightMap(CHUNK_MAP_EXTENT, CHUNK_MAP_EXTENT, ChunkOriginAxis(_coord.m_x), ChunkOriginAxis(_coord.m_z), LARGE_MAP_CELL_SIZE,
                  LARGE_MAP_CELL_SIZE, _outsideHeight),
      m_normalMap(CHUNK_MAP_EXTENT, CHUNK_MAP_EXTENT, ChunkOriginAxis(_coord.m_x), ChunkOriginAxis(_coord.m_z), LARGE_MAP_CELL_SIZE,
                  LARGE_MAP_CELL_SIZE, DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)),
      m_minHeight(_outsideHeight),
      m_maxHeight(_outsideHeight)
  {
  }


  float TerrainChunk::GetHeightSample(int _x, int _z) const
  {
    // Array2D bounds-checks and answers its outside value, so an index off the
    // chunk is defined rather than a read past the end.
    return m_heightMap.GetData(static_cast<unsigned short>(_x), static_cast<unsigned short>(_z));
  }


  void TerrainChunk::PutHeightSample(int _x, int _z, float _height)
  {
    m_heightMap.PutData(static_cast<unsigned short>(_x), static_cast<unsigned short>(_z), _height);
  }


  // The coarse envelope the residency policy keeps resident for every chunk,
  // generated or not, so a ray or line-of-sight query can be answered
  // conservatively without forcing generation. Recomputed from the samples
  // rather than tracked incrementally: generation fills a chunk in one pass and
  // then it is immutable, so there is nothing to keep in step.
  void TerrainChunk::RecalculateHeightBounds()
  {
    float lowest = FLT_MAX;
    float highest = -FLT_MAX;

    for (int z = 0; z < CHUNK_SAMPLES; ++z)
    {
      for (int x = 0; x < CHUNK_SAMPLES; ++x)
      {
        float const height = GetHeightSample(x, z);
        if (height < lowest)
        {
          lowest = height;
        }
        if (height > highest)
        {
          highest = height;
        }
      }
    }

    m_minHeight = lowest;
    m_maxHeight = highest;
  }
} // namespace Species
