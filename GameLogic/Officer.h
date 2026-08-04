
#pragma once

#include <vector>

#include "Entity.h"
#include "Flag.h"

#define OFFICER_ATTACKRANGE 10.0f
#define OFFICER_ABSORBRANGE 10.0f


class Officer : public Entity
{
  public:
    enum
    {
      StateIdle,
      StateToWaypoint,
      StateGivingOrders
    };

    enum
    {
      OrderNone,
      OrderGoto,
      OrderFollow,
      NumOrderTypes
    };

    int m_state;
    DirectX::XMFLOAT3 m_wayPoint{0.0f, 0.0f, 0.0f};
    int m_wayPointTeleportId; // Id of teleport we wish to walk into

    int m_shield;
    bool m_demoted;
    bool m_absorb;
    float m_absorbTimer;

    int m_orders;
    DirectX::XMFLOAT3 m_orderPosition{0.0f, 0.0f, 0.0f}; // Position in the world
    int m_ordersBuildingId;                              // Id of target building eg Teleport

    ShapeMarker* m_flagMarker;
    Flag m_flag;

  protected:
    bool AdvanceIdle();
    bool AdvanceToWaypoint();
    bool AdvanceGivingOrders();
    bool AdvanceToTargetPosition();

    bool SearchForRandomPosition();

    void Absorb();

    void RenderFlag(float _predictionTime);
    void RenderShield(float _predictionTime);
    void RenderSpirit(DirectX::XMFLOAT3 const& _pos);

  public:
    Officer();
    ~Officer();

    void Begin();
    void Render(float _predictionTime);
    bool RenderPixelEffect(float _predictionTime);

    bool Advance(Unit* _unit);

    void ChangeHealth(int amount);

    void SetWaypoint(DirectX::XMFLOAT3 const& _wayPoint);
    void SetOrders(DirectX::XMFLOAT3 const& _orders);
    // void SetDirectOrders( Vector3 const &_orders ); // orders while in direct control mode - removes Goto command

    void SetNextMode();
    void SetPreviousMode();

    void CancelOrderSounds();
    void ListSoundEvents(std::vector<const char*>* _list);

    static char const* GetOrderType(int _orderType);
};


class OfficerOrders : public WorldObject
{
  public:
    DirectX::XMFLOAT3 m_wayPoint{0.0f, 0.0f, 0.0f};
    float m_arrivedTimer;

  public:
    OfficerOrders();

    bool Advance();
    void Render(float _time);
};
