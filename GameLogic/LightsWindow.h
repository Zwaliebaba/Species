#ifndef _INCLUDED_LIGHTS_WINDOW_H
#define _INCLUDED_LIGHTS_WINDOW_H

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

#endif
