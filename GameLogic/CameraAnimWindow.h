#ifndef INCLUDED_CAMERA_ANIM_WINDOW_H
#define INCLUDED_CAMERA_ANIM_WINDOW_H


#ifdef LOCATION_EDITOR

#include "DarwiniaWindow.h"


// ****************************************************************************
// Class CameraAnimMainEditWindow
// ****************************************************************************

class CameraAnimMainEditWindow: public DarwiniaWindow
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

class CameraAnimSecondaryEditWindow: public DarwiniaWindow
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

#endif
