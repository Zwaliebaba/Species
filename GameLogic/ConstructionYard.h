
#pragma once

#include "Building.h"


// ****************************************************************************
// Class ConstructionYard
// ****************************************************************************

#define YARD_NUMPRIMITIVES 9
#define YARD_NUMRUNGSPIKES 6


namespace Species
{
  class ConstructionYard : public Building
  {
    protected:
      Shape* m_rung;
      Shape* m_primitive;
      ShapeMarker* m_primitives[YARD_NUMPRIMITIVES];
      ShapeMarker* m_rungSpikes[YARD_NUMRUNGSPIKES];

      int m_numPrimitives;
      int m_numSurges;
      int m_numTanksProduced;
      float m_fractionPopulated;
      float m_timer;

      float m_alpha;

      bool IsPopulationLocked(); // Are there too many tanks already

    public:
      ConstructionYard();

      bool Advance();

      void Render(float _predictionTime);
      void RenderAlphas(float _predictionTime);

      DirectX::XMFLOAT4X4 GetRungMatrix1();
      DirectX::XMFLOAT4X4 GetRungMatrix2();

      bool AddPrimitive();
      void AddPowerSurge();
  };


  // ****************************************************************************
  // Class DisplayScreen
  // ****************************************************************************

#define DISPLAYSCREEN_NUMRAYS 3

class DisplayScreen : public Building
{
  protected:
    Shape* m_armour;
    ShapeMarker* m_rays[DISPLAYSCREEN_NUMRAYS];

  public:
    DisplayScreen();

    void RenderAlphas(float _predictionTime);
};
} // namespace Species
