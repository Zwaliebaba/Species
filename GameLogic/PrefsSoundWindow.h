
#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class PrefsSoundWindow : public SpeciesWindow
  {
    public:
      int m_soundLib;
      int m_mixFreq;
      int m_numChannels;
      int m_swapStereo;
      int m_dspEffects;
      int m_memoryUsage;

    public:
      PrefsSoundWindow();

      void Create();
      void Render(bool _hasFocus);
  };
} // namespace Species
