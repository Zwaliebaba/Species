
#pragma once

#include <vector>

#include "Entity.h"

#define CENTIPEDE_MINSEARCHRANGE 75.0f
#define CENTIPEDE_MAXSEARCHRANGE 150.0f
#define CENTIPEDE_SPIRITEATRANGE 10.0f
#define CENTIPEDE_NUMSPIRITSTOREGROW 4
#define CENTIPEDE_MAXSIZE 20

namespace Neuron
{
  class Shape;
} // namespace Neuron


namespace Species
{
  class Centipede : public Entity
  {
    protected:
      float m_size;
      WorldObjectId m_next; // Guy infront of me
      WorldObjectId m_prev; // Guy behind me

      // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does
      // not, and Advance tests m_targetPos against zero to decide whether to
      // pick a new target -- so the zero is load-bearing, not tidiness.
      DirectX::XMFLOAT3 m_targetPos{0.0f, 0.0f, 0.0f};
      WorldObjectId m_targetEntity;

      std::vector<DirectX::XMFLOAT3> m_positionHistory;
      bool m_linked;
      float m_panic;
      int m_numSpiritsEaten;
      float m_lastAdvance;

      static Shape* s_shapeBody;
      static Shape* s_shapeHead;

    protected:
      bool SearchForRandomPosition();
      bool SearchForTargetEnemy();
      bool SearchForSpirits();
      bool SearchForRetreatPosition();

      bool AdvanceToTargetPosition();
      void RecordHistoryPosition();
      bool GetTrailPosition(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _vel, int _numSteps);

      void Panic(float _time);
      void EatSpirits();

    public:
      Centipede();

      void Begin();
      bool Advance(Unit* _unit);
      void ChangeHealth(int _amount);
      void Render(float _predictionTime);
      bool RenderPixelEffect(float _predictionTime);

      bool IsInView();

      void Attack(DirectX::XMFLOAT3 const& _pos);

      // The centipede's basis levelled against the world up, with m_size folded
      // into the three basis rows. The death explosion built it inline.
      DirectX::XMFLOAT4X4 XM_CALLCONV GetScaledLevelMatrix(DirectX::FXMVECTOR _position) const;

      void ListSoundEvents(std::vector<const char*>* _list);
  };
} // namespace Species
