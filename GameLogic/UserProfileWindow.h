
#pragma once

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
