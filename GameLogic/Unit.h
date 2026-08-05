#pragma once

#include "NeuronMath.h"
#include "SliceWalker.h"
#include "SlotMap.h"

#include "Entity.h"


namespace Species
{
  class Unit
  {
    protected:
      DirectX::XMFLOAT3 m_wayPoint{0.0f, 0.0f, 0.0f};

    public:
      int m_routeId;
      int m_routeWayPointId;
      int m_teamId;
      int m_unitId;
      int m_troopType;
      FastSlotMap<Entity*> m_entities;

      // The slice bookkeeping the legacy sliced array carried as a base class.
      // It is a sibling now, so the container is a plain slot map and the
      // scheduling policy belongs to whoever advances it. Public because
      // Renderer reads GetLastUpdated to decide which entities need a frame of
      // extrapolation, exactly as it did through the container.
      SliceWalker m_entitiesWalker;

      // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does
      // not, and Unit's constructor assigns only m_centrePos.
      DirectX::XMFLOAT3 m_centrePos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};
      float m_radius;

      DirectX::XMFLOAT3 m_targetDir{0.0f, 0.0f, 0.0f};

      float m_attackAccumulator; // Used to regulate fire rate

      enum
      {
        FormationRectangle,
        FormationAirstrike,
        NumFormations
      };

      DirectX::XMFLOAT3 GetWayPoint();
      virtual void SetWayPoint(DirectX::XMFLOAT3 const& _pos);
      DirectX::XMFLOAT3 GetFormationOffset(int _formation, int _index);
      DirectX::XMFLOAT3 GetOffset(int _formation, int _index); // Takes into account formation AND obstructions

    protected:
      DirectX::XMFLOAT3 m_accumulatedCentre{0.0f, 0.0f, 0.0f};
      float m_accumulatedRadiusSquared;
      int m_numAccumulated;

    public:
      Unit(int troopType, int teamId, int unitId, int numEntities, DirectX::XMFLOAT3 const& _pos);
      virtual ~Unit();

      virtual void Begin();
      virtual bool Advance(int _slice);
      virtual void Attack(DirectX::XMFLOAT3 pos, bool withGrenade);
      virtual void AdvanceEntities(int _slice);
      virtual void Render(float _predictionTime);

      virtual bool IsInView();

      Entity* NewEntity(int* _index);
      int AddEntity(Entity* _entity);
      void RemoveEntity(int _index, float _posX, float _posZ);
      int NumEntities();
      int NumAliveEntities(); // Does not count entities still in the unit, but their m_dead=true
      void UpdateEntityPosition(DirectX::XMFLOAT3 pos, float _radius);
      void RecalculateOffsets();

      void FollowRoute();

      virtual void DirectControl(TeamControls const& _teamControls);

      Entity* RayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir);
  };
} // namespace Species
