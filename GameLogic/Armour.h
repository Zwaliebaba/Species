
#pragma once

#include <vector>

#include "Entity.h"
#include "Flag.h"

class ShapeMarker;
class Shape;

#define ARMOUR_UNLOADPERIOD 0.1f


class Armour : public Entity
{
  protected:
    ShapeMarker* m_markerEntrance;
    ShapeMarker* m_markerFlag;
    // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does not.
    DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_conversionPoint{0.0f, 0.0f, 0.0f};
    float m_speed;

    enum
    {
      StateIdle,
      StateUnloading,
      StateLoading,
    };
    int m_numPassengers;
    float m_previousUnloadTimer;
    float m_newOrdersTimer;

    Flag m_flag;
    Flag m_deployFlag;

  public:
    DirectX::XMFLOAT3 m_wayPoint{0.0f, 0.0f, 0.0f};
    int m_state;

  public:
    Armour();
    ~Armour();

    void Begin();
    bool Advance(Unit* _unit);
    void Render(float _predictionTime);

    void ChangeHealth(int _amount);

    void SetOrders(DirectX::XMFLOAT3 const& _orders);
    void SetWayPoint(DirectX::XMFLOAT3 const& _wayPoint);
    void SetConversionPoint(DirectX::XMFLOAT3 const& _conversionPoint);
    void AdvanceToTargetPos();
    void DetectCollisions();
    void ConvertToGunTurret();

    void SetDirectOrders(); // orders while in direct control mode - removes gun turret mode

    bool IsLoading();
    bool IsUnloading();
    void ToggleLoading();
    void AddPassenger();
    void RemovePassenger();

    int Capacity();

    void ListSoundEvents(std::vector<const char*>* _list);

    void GetEntrance(DirectX::XMFLOAT3& _exitPos, DirectX::XMFLOAT3& _exitDir);
};
