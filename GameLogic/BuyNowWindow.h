#pragma once

#include "SpeciesWindow.h"

class BuyNowWindow : public SpeciesWindow {
public:

	BuyNowWindow();

	void Create();
    void Render(bool _hasFocus);
};
