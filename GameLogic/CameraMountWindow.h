#ifndef _INCLUDED_CAMERA_MOUNT_WINDOW_H
#define _INCLUDED_CAMERA_MOUNT_WINDOW_H


#ifdef LOCATION_EDITOR

#include "DarwiniaWindow.h"



// ****************************************************************************
// Class CameraMountEditWindow
// ****************************************************************************

class CameraMountEditWindow: public DarwiniaWindow
{
public:
    CameraMountEditWindow( char const *name );
	~CameraMountEditWindow();

	void Create();
};


#endif // LOCATION_EDITOR

#endif
