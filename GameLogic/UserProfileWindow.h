
#ifndef _included_userprofilewindow_h
#define _included_userprofilewindow_h

#include "SpeciesWindow.h"


class UserProfileWindow : public SpeciesWindow
{
public:
    UserProfileWindow();

    void Render ( bool hasFocus );
    void Create();
};


class NewUserProfileWindow : public SpeciesWindow
{
public:
    static char s_profileName[256];

public:
    NewUserProfileWindow();
    void Create();
};

#endif