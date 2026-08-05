#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class GlobalWorldEditorWindow : public SpeciesWindow
  {
    public:
      GlobalWorldEditorWindow();

      void Create();
      void Update();
  };
} // namespace Species
