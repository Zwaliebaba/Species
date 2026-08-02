#pragma once

#ifdef LOCATION_EDITOR

#include "SpeciesWindow.h"


// ****************************************************************************
// Class LightsEditWindow
// ****************************************************************************

class LightsEditWindow: public SpeciesWindow
{
public:
    LightsEditWindow( char const *name );
	~LightsEditWindow();

	void Create();
};

#endif // LOCATION_EDITOR

