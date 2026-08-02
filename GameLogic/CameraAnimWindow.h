#pragma once


#ifdef LOCATION_EDITOR

#include "SpeciesWindow.h"


// ****************************************************************************
// Class CameraAnimMainEditWindow
// ****************************************************************************

class CameraAnimMainEditWindow: public SpeciesWindow
{
public:
    CameraAnimMainEditWindow( char const *name );
	~CameraAnimMainEditWindow();

	void Create();
	void RemoveButtons();
	void AddButtons();
};


// ****************************************************************************
// Class CameraAnimSecondaryEditWindow
// ****************************************************************************

class CameraAnimSecondaryEditWindow: public SpeciesWindow
{
public:
	int m_animId;
	bool m_newNodeArmed;

    CameraAnimSecondaryEditWindow(char *name, int _animId);
	~CameraAnimSecondaryEditWindow();

	void Create();
	void RemoveButtons();
	void AddButtons();
};


#endif // LOCATION_EDITOR

