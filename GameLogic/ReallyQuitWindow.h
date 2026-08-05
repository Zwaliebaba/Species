#pragma once

#include "SpeciesWindow.h"

#define REALLYQUIT_WINDOWNAME "Really Quit?"


namespace Species
{
  class ReallyQuitWindow : public SpeciesWindow
  {
    public:
      ReallyQuitWindow();
      void Create();
  };
} // namespace Species
