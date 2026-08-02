#ifndef _included_EditWindow_h
#define _included_EditWindow_h

#ifdef LOCATION_EDITOR

#include "SpeciesWindow.h"


class MainEditWindow : public SpeciesWindow
{
public:
	SpeciesWindow *m_currentEditWindow;

	MainEditWindow( char const *name );

	void Create();
};

#endif // LOCATION_EDITOR


#endif
