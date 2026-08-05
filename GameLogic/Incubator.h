#pragma once

#include <vector>

#include "SlotMap.h"
#include "Building.h"
#include "Spirit.h"


#define INCUBATOR_PROCESSTIME 5.0f

namespace Neuron
{
  class ShapeMarker;
} // namespace Neuron


namespace Species
{
  struct IncubatorIncoming
  {
      // Braced to zero: Vector3's default constructor did it and XMFLOAT3's does
      // not. These are value-initialised into a vector, so nothing else would.
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      int m_entrance;
      float m_alpha;
  };

  class Incubator : public Building
  {
    protected:
      FastSlotMap<Spirit> m_spirits;
      ShapeMarker* m_spiritCentre;
      ShapeMarker* m_exit;
      ShapeMarker* m_dock;
      ShapeMarker* m_spiritEntrance[3];

      int m_troopType;
      float m_timer;

      std::vector<IncubatorIncoming*> m_incoming;

    public:
      int m_numStartingSpirits;

      Incubator();
      ~Incubator() override;

      void Initialise(Building* _template) override;

      bool Advance() override;
      void SpawnEntity();
      void AddSpirit(Spirit* _spirit);

      void Render(float _predictionTime) override;
      void RenderAlphas(float _predictionTime) override;

      int NumSpiritsInside();

      void Read(TextReader* _in, bool _dynamic) override;
      void Write(FileWriter* _out) override;

      void GetDockPoint(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front);

      void ListSoundEvents(std::vector<const char*>* _list) override;
  };
} // namespace Species
