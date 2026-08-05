
#pragma once

#include <string>

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
  static std::string s_profileName;

public:
    NewUserProfileWindow();
    void Create();
};
