#include "pch.h"
#include <float.h>

#include "MathUtils.h"
#include "Debug.h"
#include "DebugRender.h"

#include "Landscape.h"
#include "RoutingSystem.h"
#include "Location.h"

#include "RadarDish.h"
#include "WorldPointers.h"


// ****************************************************************************
// Class WayPoint
// ****************************************************************************

// *** Constructor


namespace Species
{
  WayPoint::WayPoint(int _type, DirectX::XMFLOAT3 const& _pos)
    : m_type(_type),
      m_pos(_pos),
      m_buildingId(-1)
  {
  }


  // *** Destructor
  WayPoint::~WayPoint() {}


  // *** GetPos
  // I could have made it so that the Y values for GroundPoses were set once at
  // level load time, but that would require an init phases after level file
  // parsing and landscape terrain generation.
  DirectX::XMFLOAT3 WayPoint::GetPos()
  {
    DirectX::XMFLOAT3 rv(m_pos);

    if (m_type == TypeGroundPos)
    {
      Landscape* land = &g_location->m_landscape;
      rv.y = land->m_heightMap->GetValue(rv.x, rv.z);
      if (rv.y < 0.0f)
      {
        rv.y = 0.0f;
      }
      rv.y += 1.0f;
    }
    else if (m_type == TypeBuilding)
    {
      Building* building = g_location->GetBuilding(m_buildingId);
      if (building)
      {
        DEBUG_ASSERT(building->m_type == Building::TypeRadarDish || building->m_type == Building::TypeBridge);
        Teleport* teleport = (Teleport*)building;
        DirectX::XMFLOAT3 pos, front;
        teleport->GetEntrance(pos, front);
        return pos;
      }
    }

    return rv;
  }


  // *** SetPos
  void WayPoint::SetPos(DirectX::XMFLOAT3 const& _pos) { m_pos = _pos; }


  // ****************************************************************************
  // Class Route
  // ****************************************************************************

  // *** Constructor
  Route::Route(int _id)
    : m_id(_id)
  {
  }


  // *** Destructor
  Route::~Route() = default;


  void Route::AddWayPoint(DirectX::XMFLOAT3 const& _pos) { m_wayPoints.push_back(std::make_unique<WayPoint>(WayPoint::Type3DPos, _pos)); }


  void Route::AddWayPoint(int _buildingId)
  {
    auto wayPoint = std::make_unique<WayPoint>(WayPoint::TypeBuilding, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
    wayPoint->m_buildingId = _buildingId;
    m_wayPoints.push_back(std::move(wayPoint));
  }


  WayPoint* Route::GetWayPoint(int _id)
  {
    if (ValidIndex(m_wayPoints, _id))
    {
      return m_wayPoints[_id].get();
    }

    return nullptr;
  }


  int Route::GetIdOfNearestWayPoint(DirectX::XMFLOAT3 const& _pos)
  {
    int idOfNearest = -1;
    float distToNearestSqrd = FLT_MAX;

    int size = static_cast<int>(m_wayPoints.size());
    for (int i = 0; i < size; ++i)
    {
      WayPoint* wp = m_wayPoints[i].get();
      DirectX::XMFLOAT3 const wayPointPos = wp->GetPos();
      float distSqrd = DirectX::XMVectorGetX(
        DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_pos), DirectX::XMLoadFloat3(&wayPointPos))));
      if (distSqrd < distToNearestSqrd)
      {
        distToNearestSqrd = distSqrd;
        idOfNearest = i;
      }
    }

    return idOfNearest;
  }


  // Returns the id of the first waypoint of the nearest edge
  int Route::GetIdOfNearestEdge(DirectX::XMFLOAT3 const& _pos, float* _dist)
  {
    int idOfNearest = 0;
    float distToNearest = FLT_MAX;

    DirectX::XMFLOAT2 pos(_pos.x, _pos.z);
    WayPoint* wp = m_wayPoints[0].get();
    DirectX::XMFLOAT3 newPos = wp->GetPos();
    DirectX::XMFLOAT2 oldPos(newPos.x, newPos.z);

    int size = static_cast<int>(m_wayPoints.size());
    for (int i = 1; i < size; ++i)
    {
      wp = m_wayPoints[i].get();
      newPos = wp->GetPos();
      DirectX::XMFLOAT2 temp(newPos.x, newPos.z);
      float dist = PointSegDist2D(pos, oldPos, temp);
      oldPos = temp;

      if (dist < distToNearest)
      {
        distToNearest = dist;
        idOfNearest = i;
      }
    }

    return idOfNearest - 1;
  }


  void Route::Render()
  {
    if (!g_location)
      return;

#ifdef DEBUG_RENDER_ENABLED
  DirectX::XMFLOAT3 lastPos{0.0f, 0.0f, 0.0f};

  glDisable(GL_DEPTH_TEST);

  for (int i = 0; i < static_cast<int>(m_wayPoints.size()); ++i)
  {
    WayPoint* wayPoint = m_wayPoints[i].get();
    DirectX::XMFLOAT3 const thisPos = wayPoint->GetPos();
    if (i > 0)
    {
      RenderArrow(lastPos, thisPos, 5.0f, RGBAColour(255, 100, 100, 255));
    }
    lastPos = thisPos;
  }

  glEnable(GL_DEPTH_TEST);
#endif
  }
} // namespace Species
