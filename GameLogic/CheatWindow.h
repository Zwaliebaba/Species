
#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class CheatWindow : public SpeciesWindow
  {
    public:
      CheatWindow(char const* _name);

      void Create();
  };
} // namespace Species
