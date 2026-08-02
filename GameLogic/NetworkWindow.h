
#pragma once

#include "SpeciesWindow.h"


class NetworkWindow : public SpeciesWindow
{
public:
    NetworkWindow( char const *name );

    void Render( bool hasFocus );
};


