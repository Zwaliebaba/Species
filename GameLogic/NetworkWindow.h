
#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class NetworkWindow : public SpeciesWindow
  {
    public:
      NetworkWindow(char const* name);

      void Render(bool hasFocus);
  };
} // namespace Species
