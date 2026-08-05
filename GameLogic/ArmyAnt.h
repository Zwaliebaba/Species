
#pragma once

#include "Entity.h"

#define ARMYANT_SEARCHRANGE 10


class ArmyAnt : public Entity
{
  protected:
    float m_scale;
    Shape* m_shapes[3];
    ShapeMarker* m_carryMarker;

    bool AdvanceScoutArea();
    bool AdvanceCollectSpirit();
    bool AdvanceCollectEntity();
    bool AdvanceAttackEnemy();
    bool AdvanceReturnToBase();
    bool AdvanceToTargetPosition();
    bool AdvanceBaseDestroyed();

    bool SearchForTargets();
    bool SearchForSpirits();
    bool SearchForEnemies();
    bool SearchForAntHill();
    bool SearchForRandomPosition();

  public:
    // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does not.
    DirectX::XMFLOAT3 m_wayPoint{0.0f, 0.0f, 0.0f};
    int m_orders;
    bool m_targetFound;
    int m_spiritId;
    WorldObjectId m_targetId;

    enum
    {
      NoOrders,
      ScoutArea,
      CollectSpirit,
      CollectEntity,
      AttackEnemy,
      ReturnToBase,
      BaseDestroyed
    };

  public:
    ArmyAnt();

    void Begin();
    bool Advance(Unit* _unit);
    void ChangeHealth(int _amount);
    void Render(float _predictionTime);

    void OrderReturnToBase();

    void GetCarryMarker(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _vel);

    // The ant's basis levelled against the world up, with m_scale folded into
    // the three basis rows. Both the death explosion and Render built it.
    DirectX::XMFLOAT4X4 GetScaledLevelMatrix() const;
};
