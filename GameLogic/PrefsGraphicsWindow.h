
#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class PrefsGraphicsWindow : public SpeciesWindow
  {
    public:
      int m_landscapeDetail;
      int m_waterDetail;
      int m_cloudDetail;
      int m_buildingDetail;
      int m_entityDetail;
      int m_pixelEffectRange;

    public:
      PrefsGraphicsWindow();
      ~PrefsGraphicsWindow();

      void Create();
      void Render(bool _hasFocus);
  };
} // namespace Species
