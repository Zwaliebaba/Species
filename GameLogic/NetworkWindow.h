
#ifndef _included_networkwindow_h
#define _included_networkwindow_h

#include "DarwiniaWindow.h"


class NetworkWindow : public DarwiniaWindow
{
public:
    NetworkWindow( char const *name );

    void Render( bool hasFocus );
};


#endif
