#pragma once

#include "NeuronMath.h"

#include "SlotMap.h"

#include "Spirit.h"


namespace Species
{
  class SpiritStore
  {
    public:
      // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does
      // not, and SpiritStore's constructor assigns it only in Initialise.
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      float m_sizeX;
      float m_sizeY;
      float m_sizeZ;

    protected:
      FastSlotMap<Spirit> m_spirits;

    public:
      SpiritStore();

      void Initialise(int _initialCapacity, int _maxCapacity, DirectX::XMFLOAT3 _pos, float _sizeX, float _sizeY,
                      float _sizeZ); // Capacity isn't enforced, just provide a "best guess"

      void Advance();
      void Render(float _predictionTime);

      int NumSpirits();
      void AddSpirit(Spirit* _spirit);
      void RemoveSpirits(int _quantity);
  };
} // namespace Species
