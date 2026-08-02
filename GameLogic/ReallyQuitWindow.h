#ifndef _included_reallyquit_window_h
#define _included_reallyquit_window_h

#include "SpeciesWindow.h"

#define REALLYQUIT_WINDOWNAME "Really Quit?"

class ReallyQuitWindow : public SpeciesWindow {
public:
	ReallyQuitWindow();
	void Create();
};

#endif // _included_reallyquit_window_h