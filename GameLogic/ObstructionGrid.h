
#pragma once

#include "2dArray.h"
#include "2dSurfaceMap.h"
#include "LList.h"


class ObstructionGridCell
{
public:
    LList<int> m_buildings;
};



class ObstructionGrid
{
protected:
    SurfaceMap2D <ObstructionGridCell> m_cells;

    void CalculateBuildingArea( int _buildingId );              // This cannot be called once on its own
                                                                // It must be called as part of a complete recalc

public:
    ObstructionGrid( float _cellSizeX, float _cellSizeZ );

    void CalculateAll();

    LList<int> *GetBuildings( float _locationX, float _locationZ );

    void Render();
};



