#include "pch.h"

#include "RoutingSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// GameLogic is in namespace Species since namespace-migration T4, and unlike
// namespace Neuron it has no tree-wide using-directive to reach it by. A test
// source is the right place for one: it is a .cpp, so nothing includes it.
using namespace Species;

namespace GameLogicTests
{
  // Routes are what units follow: an entity carries m_routeId and
  // m_routeWayPointId, and the waypoint id is a position in the route's LList.
  // So the ordering asserted here is not a convenience — advancing along a
  // route means incrementing an index into this list, and a container change
  // that reorders it moves every unit already walking.
  //
  // Only Type3DPos waypoints appear here. The other two kinds resolve their
  // position against the live world — TypeGroundPos samples the landscape
  // heightmap, TypeBuilding looks the building up in the Location — and need a
  // loaded level rather than a constructed object.
  TEST_CLASS(RouteTests)
  {
    public:
      TEST_METHOD(WayPointsKeepInsertionOrder)
      {
        Route route(7);
        route.AddWayPoint(DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f));
        route.AddWayPoint(DirectX::XMFLOAT3(20.0f, 0.0f, 0.0f));
        route.AddWayPoint(DirectX::XMFLOAT3(30.0f, 0.0f, 0.0f));

        Assert::AreEqual(size_t(3), route.m_wayPoints.size());
        Assert::AreEqual(10.0f, route.GetWayPoint(0)->GetPos().x, 0.0001f);
        Assert::AreEqual(20.0f, route.GetWayPoint(1)->GetPos().x, 0.0001f);
        Assert::AreEqual(30.0f, route.GetWayPoint(2)->GetPos().x, 0.0001f);
      }

      TEST_METHOD(GetWayPointRejectsAnIndexOffTheEnd)
      {
        Route route(7);
        route.AddWayPoint(DirectX::XMFLOAT3(10.0f, 0.0f, 0.0f));

        Assert::IsNull(route.GetWayPoint(1));
        Assert::IsNull(route.GetWayPoint(-1));
      }

      TEST_METHOD(NearestWayPointIsByDistance)
      {
        Route route(7);
        route.AddWayPoint(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        route.AddWayPoint(DirectX::XMFLOAT3(100.0f, 0.0f, 0.0f));
        route.AddWayPoint(DirectX::XMFLOAT3(200.0f, 0.0f, 0.0f));

        Assert::AreEqual(1, route.GetIdOfNearestWayPoint(DirectX::XMFLOAT3(90.0f, 0.0f, 0.0f)));
        Assert::AreEqual(2, route.GetIdOfNearestWayPoint(DirectX::XMFLOAT3(1000.0f, 0.0f, 0.0f)));
      }

      TEST_METHOD(AnEmptyRouteHasNoNearestWayPoint)
      {
        Route route(7);

        Assert::AreEqual(-1, route.GetIdOfNearestWayPoint(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)));
      }

      TEST_METHOD(BuildingWayPointsRememberTheirBuilding)
      {
        // The building's position is looked up lazily through the Location, so
        // this asserts only what is recorded at insertion time.
        Route route(7);
        route.AddWayPoint(12);

        Assert::AreEqual(int(WayPoint::TypeBuilding), route.GetWayPoint(0)->m_type);
        Assert::AreEqual(12, route.GetWayPoint(0)->m_buildingId);
      }
  };
} // namespace GameLogicTests
