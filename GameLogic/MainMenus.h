
#pragma once

#include "SpeciesWindow.h"


class MainMenuWindow : public SpeciesWindow
{
public:
    MainMenuWindow();

    void Create();
	void Render( bool _hasFocus );
};


class OptionsMenuWindow : public SpeciesWindow
{
public:
    OptionsMenuWindow();

    void Create();
};


class LocationWindow : public SpeciesWindow
{
public:
    LocationWindow();

    void Create();
};


class ResetLocationWindow : public SpeciesWindow
{
public:
    ResetLocationWindow();

    void Create();
    void Render( bool _hasFocus );
};


class AboutSpeciesWindow : public SpeciesWindow
{
public:
    AboutSpeciesWindow();

    void Create();
    void Render( bool _hasFocus );
};

class SkipPrologueWindow : public SpeciesWindow
{
public:
	SkipPrologueWindow();

	void Create();
	void Render( bool _hasFocus );
};

class PlayPrologueWindow : public SpeciesWindow
{
public:
	PlayPrologueWindow();

	void Create();
	void Render ( bool _hasFocus );
};
