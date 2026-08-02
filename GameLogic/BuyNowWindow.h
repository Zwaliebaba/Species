#ifndef __BUYNOW_WINDOW_H
#define __BUYNOW_WINDOW_H

#include "SpeciesWindow.h"

class BuyNowWindow : public SpeciesWindow {
public:

	BuyNowWindow();

	void Create();
    void Render(bool _hasFocus);
};

#endif // __BUYNOW_WINDOW_H