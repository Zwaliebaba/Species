#pragma once

#include <vector>

#include "Unit.h"


class AirstrikeUnit : public Unit
{
  public:
    DirectX::XMFLOAT3 m_enterPosition{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_attackPosition{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_exitPosition{0.0f, 0.0f, 0.0f};

    DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f}; // Current direction
    DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
    float m_speed;

    int m_effectId;
    int m_numInvaders;

    enum
    {
      StateApproaching,
      StateLeaving
    };
    int m_state;

    bool AdvanceToTargetPosition(DirectX::XMFLOAT3 _targetPos);

  public:
    AirstrikeUnit(int teamId, int unitId, int numEntities, DirectX::XMFLOAT3 const& _pos);

    void Begin();
    bool Advance(int _slice);
    void Render(float _predictionTime);

    bool IsInView();
};


class SpaceInvader : public Entity
{
  protected:
    DirectX::XMFLOAT3 m_targetPos{0.0f, 0.0f, 0.0f};
    bool m_armed;
    Shape* m_bombShape;

  public:
    SpaceInvader();

    bool Advance(Unit* _unit);
    void ChangeHealth(int _amount);
    void Render(float _predictionTime);

    void ListSoundEvents(std::vector<const char*>* _list);
};
