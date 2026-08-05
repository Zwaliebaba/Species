
#pragma once

#include <vector>

#include "Entity.h"
#include "SlotMap.h"

#define SOULDESTROYER_MINSEARCHRANGE 200.0f
#define SOULDESTROYER_MAXSEARCHRANGE 300.0f
#define SOULDESTROYER_DAMAGERANGE 25.0f
#define SOULDESTROYER_MAXSPIRITS 50

namespace Neuron
{
  class Shape;
} // namespace Neuron


namespace Species
{
  class SoulDestroyer : public Entity
  {
    protected:
      // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does
      // not, and Advance tests m_targetPos against zero to decide whether to
      // retarget -- so the zero is load-bearing here, not tidiness.
      DirectX::XMFLOAT3 m_targetPos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
      WorldObjectId m_targetEntity;
      std::vector<DirectX::XMFLOAT3> m_positionHistory;
      FastSlotMap<float> m_spirits;

      float m_retargetTimer;
      float m_panic;

      static Shape* s_shapeHead;
      static Shape* s_shapeTail;
      static ShapeMarker* s_tailMarker;

      DirectX::XMFLOAT3 m_spiritPosition[SOULDESTROYER_MAXSPIRITS];

    protected:
      bool SearchForRandomPosition();
      bool SearchForTargetEnemy();
      bool SearchForRetreatPosition();

      bool AdvanceToTargetPosition();
      void RecordHistoryPosition();
      bool GetTrailPosition(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _vel);

      void RenderShapes(float _predictionTime);
      void RenderShapesForPixelEffect(float _predictionTime);
      void RenderSpirit(DirectX::XMFLOAT3 const& _pos, float _alpha);
      bool RenderPixelEffect(float _predictionTime);

      void Panic(float _time);

    public:
      SoulDestroyer();

      void Begin();
      bool Advance(Unit* _unit);
      void ChangeHealth(int _amount);
      void Render(float _predictionTime);

      void Attack(DirectX::XMFLOAT3 const& _pos);

      void ListSoundEvents(std::vector<const char*>* _list);

      void SetWaypoint(DirectX::XMFLOAT3 const _waypoint);
  };


  class Zombie : public WorldObject
  {
    public:
      // Zombie's own basis. Braced to zero for the same reason as above.
      DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
      float m_life;

      DirectX::XMFLOAT3 m_hover{0.0f, 0.0f, 0.0f};
      float m_positionOffset; // Used to make them float around a bit
      float m_xaxisRate;
      float m_yaxisRate;
      float m_zaxisRate;

    public:
      Zombie();

      bool Advance();
      void Render(float _predictionTime);
  };
} // namespace Species
