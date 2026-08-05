#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class BuyNowWindow : public SpeciesWindow
  {
    public:
      BuyNowWindow();

      void Create();
      void Render(bool _hasFocus);
  };
} // namespace Species
