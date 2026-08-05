
#pragma once

#include <vector>

#include "Entity.h"

#define SPOREGENERATOR_NUMTAILS 4


class SporeGenerator : public Entity
{
  public:
    float m_retargetTimer;
    float m_eggTimer;
    // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does not.
    DirectX::XMFLOAT3 m_targetPos{0.0f, 0.0f, 0.0f};
    int m_spiritId;

  protected:
    bool SearchForRandomPos();
    bool SearchForSpirits();

    bool AdvanceToTargetPosition();
    bool AdvanceIdle();
    bool AdvanceEggLaying();
    bool AdvancePanic();

    void RenderTail(DirectX::XMFLOAT3 const& _from, DirectX::XMFLOAT3 const& _to, float _size);

  protected:
    ShapeMarker* m_eggMarker;
    ShapeMarker* m_tail[SPOREGENERATOR_NUMTAILS];

    enum
    {
      StateIdle,
      StateEggLaying,
      StatePanic
    };
    int m_state;

  public:
    SporeGenerator();

    void Begin();
    bool Advance(Unit* _unit);
    void ChangeHealth(int _amount);

    bool IsInView();
    void Render(float _predictionTime);
    bool RenderPixelEffect(float _predictionTime);

    void ListSoundEvents(std::vector<const char*>* _list);
};
