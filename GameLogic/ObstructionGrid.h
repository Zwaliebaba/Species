
#pragma once

#include "2dArray.h"
#include "2dSurfaceMap.h"
#include <vector>


namespace Species
{
  class ObstructionGridCell
  {
    public:
      std::vector<int> m_buildings;
  };


  class ObstructionGrid
  {
    protected:
      SurfaceMap2D<ObstructionGridCell> m_cells;

      void CalculateBuildingArea(int _buildingId); // This cannot be called once on its own
                                                   // It must be called as part of a complete recalc

    public:
      ObstructionGrid(float _cellSizeX, float _cellSizeZ);

      void CalculateAll();

      // Const because the cells are: callers only ever read the ids. The legacy list
      // version cast the const away with a C-style cast to hand out a mutable
      // pointer nothing wanted.
      std::vector<int> const* GetBuildings(float _locationX, float _locationZ) const;

      void Render();
  };
} // namespace Species
