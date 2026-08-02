
#ifndef _included_networkwindow_h
#define _included_networkwindow_h

#include "SpeciesWindow.h"


class NetworkWindow : public SpeciesWindow
{
public:
    NetworkWindow( char const *name );

    void Render( bool hasFocus );
};


#endif
