#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class SaveOnQuitWindow : public SpeciesWindow
  {
    public:
      SaveOnQuitWindow(char const* _name);

      void Create();
      void Render(bool hasFocus);
  };
} // namespace Species
