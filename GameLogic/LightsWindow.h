#pragma once

#ifdef LOCATION_EDITOR

#include "SpeciesWindow.h"


// ****************************************************************************
// Class LightsEditWindow
// ****************************************************************************


namespace Species
{
  class LightsEditWindow : public SpeciesWindow
  {
    public:
      LightsEditWindow(char const* name);
      ~LightsEditWindow();

      void Create();
  };

#endif // LOCATION_EDITOR
} // namespace Species
