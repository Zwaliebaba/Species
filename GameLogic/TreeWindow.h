#pragma once


#ifdef LOCATION_EDITOR


#include "SpeciesWindow.h"


class TreeWindow : public SpeciesWindow
{
public:
    int m_selectionId;

public:
    TreeWindow( char const *_name );

    void Create();
    void Update();
};


#endif // LOCATION_EDITOR

