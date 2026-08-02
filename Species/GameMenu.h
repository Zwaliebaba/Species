
#pragma once

#include "SpeciesWindow.h"
#include "DropDownMenu.h"
#include "InputField.h"

#define MAX_GAME_TYPES 6

class GlobalInternet;

class GameMenu
{
public:

    bool            m_menuCreated;
    GlobalInternet  *m_internet;

public:
    GameMenu();

    void Render();

    void CreateMenu();
    void DestroyMenu();
};

class GameMenuButton : public SpeciesButton
{
public:
    char    *m_iconName;
public:
    GameMenuButton( char const *_iconName );
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

class GameMenuWindow : public SpeciesWindow
{
public:
    int     m_currentPage;
    int     m_newPage;


    enum
    {
        PageMain = 0,
        PageSpecies,
        PageMultiwinia,
        PageGameSetup,
        PageResearch,
        NumPages
    };

public:
    GameMenuWindow();

    void Create ();
    void Update();
    void Render ( bool _hasFocus );

    void SetupNewPage( int _page );
    void SetupMainPage();
    void SetupSpeciesPage();

    void CreateMenuControl( char const *name, int dataType, void *value, int y,
							float change, float _lowBound, float _highBound,
                            SpeciesButton *callback, int x, int w, float fontSize);

    void GetDefaultPositions( int *_x, int *_y, int *_gap );
};
