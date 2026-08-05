#pragma once


#ifdef LOCATION_EDITOR

#include "SpeciesWindow.h"



// ****************************************************************************
// Class CameraMountEditWindow
// ****************************************************************************


namespace Species
{
  class CameraMountEditWindow : public SpeciesWindow
  {
    public:
      CameraMountEditWindow(char const* name);
      ~CameraMountEditWindow();

      void Create();
  };


#endif // LOCATION_EDITOR
} // namespace Species
