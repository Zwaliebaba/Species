#ifndef _INCLUDED_INSTANT_UNIT_WINDOW_H
#define _INCLUDED_INSTANT_UNIT_WINDOW_H

#ifdef LOCATION_EDITOR

#include "SpeciesWindow.h"


// ****************************************************************************
// Class InstantUnitEditWindow
// ****************************************************************************

class InstantUnitEditWindow: public SpeciesWindow
{
public:
	InstantUnitEditWindow( char const *name );
	~InstantUnitEditWindow();

	void Create();
};


// ****************************************************************************
// Class InstantUnitCreateWindow
// ****************************************************************************

class InstantUnitCreateWindow: public SpeciesWindow
{
public:
    InstantUnitCreateWindow( char const *name );
	~InstantUnitCreateWindow();

	void Create();
};


#endif // LOCATION_EDITOR

#endif
