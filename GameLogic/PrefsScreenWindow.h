
#pragma once

#include "SpeciesWindow.h"


class PrefsScreenWindow : public SpeciesWindow
{
public:
    int     m_resId;
    int     m_windowed;
    int     m_colourDepth;
    int     m_refreshRate;
    int     m_zDepth;

public:
    PrefsScreenWindow();

    void Create();
    void Render( bool _hasFocus );
};

void SetWindowed(bool _isWindowed, bool _isPermanent, bool &_isSwitchingWindowed);
void RestartWindowManagerAndRenderer();
