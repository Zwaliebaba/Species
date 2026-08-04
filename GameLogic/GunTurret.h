#pragma once

#include <vector>

#include "Building.h"

#define GUNTURRET_RETARGETTIMER 3.0f
#define GUNTURRET_MINRANGE 100.0f
#define GUNTURRET_MAXRANGE 300.0f
#define GUNTURRET_NUMSTATUSMARKERS 5
#define GUNTURRET_NUMBARRELS 4
#define GUNTURRET_OWNERSHIPTIMER 1.0f


class GunTurret : public Building
{
  protected:
    Shape* m_turret;
    Shape* m_barrel;
    ShapeMarker* m_barrelMount;
    ShapeMarker* m_barrelEnd[GUNTURRET_NUMBARRELS];
    ShapeMarker* m_statusMarkers[GUNTURRET_NUMSTATUSMARKERS];

    // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does
    // not, and GunTurret's constructor assigns neither of these nor m_target.
    DirectX::XMFLOAT3 m_turretFront{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_barrelUp{0.0f, 0.0f, 0.0f};

    bool m_aiTargetCreated;

    DirectX::XMFLOAT3 m_target{0.0f, 0.0f, 0.0f};
    WorldObjectId m_targetId;
    float m_fireTimer;
    int m_nextBarrel;
    float m_retargetTimer;
    bool m_targetCreated;

    float m_health;
    float m_ownershipTimer;

  protected:
    void PrimaryFire();
    bool SearchForTargets();
    void SearchForRandomPos();
    void RecalculateOwnership();

  public:
    GunTurret();

    void Initialise(Building* _template);
    void SetDetail(int _detail);

    void ExplodeBody();
    void Damage(float _damage);
    bool Advance();

    DirectX::XMFLOAT3 GetTarget();

    bool IsInView();
    void Render(float _predictionTime);
    void RenderPorts();

    bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen, DirectX::XMFLOAT3* _pos,
                    DirectX::XMFLOAT3* norm);

    void ListSoundEvents(std::vector<const char*>* _list);
};


class GunTurretTarget : public WorldObject
{
  public:
    int m_buildingId;

  public:
    GunTurretTarget(int _buildingId);
    bool Advance();
    void Render(float _time);
};
