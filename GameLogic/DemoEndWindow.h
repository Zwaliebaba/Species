#ifndef _included_demoendwindow_h
#define _included_demoendwindow_h

#include "SpeciesWindow.h"


class DemoEndWindow : public SpeciesWindow
{
protected:
    float m_timer;
    float m_fadeInTime;

public:
    bool m_saveGame;

public:
    DemoEndWindow( float _fadeInTime, bool _saveGame );

    float GetAlpha();
    bool ShowExitButton();

    void Create();
    void Render( bool hasFocus );
};



#endif