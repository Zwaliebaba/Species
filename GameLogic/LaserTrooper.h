
#pragma once

#include "Entity.h"


namespace Species
{
  class LaserTrooper : public Entity
  {
    public:
      // Braced to zero: LaserTrooper has no constructor assigning these, so
      // Vector3's default one was doing it and XMFLOAT3's does not.
      DirectX::XMFLOAT3 m_targetPos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_unitTargetPos{0.0f, 0.0f, 0.0f};

      float m_victoryDance;

    public:
      void Begin();

      bool Advance(Unit* _unit);
      void Render(float _predictionTime, int _teamId);

      void AdvanceVictoryDance();
  };
} // namespace Species
