#pragma once

#include "SpeciesWindow.h"


class SaveOnQuitWindow : public SpeciesWindow
{
public:
    SaveOnQuitWindow( char const *_name );

    void Create();
	void Render(bool hasFocus);
};


