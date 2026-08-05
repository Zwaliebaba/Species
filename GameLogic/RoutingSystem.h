#pragma once


#include "NeuronMath.h"


// ****************************************************************************
// Class WayPoint
// ****************************************************************************

class WayPoint
{
  protected:
    DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};

  public:
    enum
    {
      Type3DPos,
      TypeGroundPos,
      TypeBuilding
    };

    int m_type;
    int m_buildingId;

    WayPoint(int _type, DirectX::XMFLOAT3 const& _pos);
    ~WayPoint();

    DirectX::XMFLOAT3 GetPos();
    void SetPos(DirectX::XMFLOAT3 const& _pos);
};


// ****************************************************************************
// Class Route
// ****************************************************************************

class Route
{
  public:
    int m_id;
    std::vector<WayPoint*> m_wayPoints;

    Route(int _id);
    ~Route();

    void AddWayPoint(DirectX::XMFLOAT3 const& _pos);
    void AddWayPoint(int _buildingId);
    WayPoint* GetWayPoint(int _id);

    int GetIdOfNearestWayPoint(DirectX::XMFLOAT3 const& _pos);
    int GetIdOfNearestEdge(DirectX::XMFLOAT3 const& _pos, float* _dist);

    void Render();
};
