#ifndef INCLUDED_SAVE_ON_QUIT_WINDOW_H
#define INCLUDED_SAVE_ON_QUIT_WINDOW_H

#include "SpeciesWindow.h"


class SaveOnQuitWindow : public SpeciesWindow
{
public:
    SaveOnQuitWindow( char const *_name );

    void Create();
	void Render(bool hasFocus);
};


#endif
