#include "pch.h"

#include "HiResTime.h"
#include "Input.h"
#include "KeyDefs.h"
#include "LanguageTable.h"
#include "Preferences.h"
#include "Resource.h"
#include "TargetCursor.h"
#include "TextRenderer.h"
#include "TextStreamReaders.h"

#include "DropDownMenu.h"
#include "InputField.h"

#include "GameMenu.h"

#include "App.h"
#include "Camera.h"
#include "GlobalWorld.h"
#include "GlobalInternet.h"
#include "Renderer.h"

// *************************
// Button Classes
// *************************

GameMenuButton::GameMenuButton(char const *_iconName)
{
    m_iconName = strdup(_iconName );
    m_fontSize = 65.0f;
}

void GameMenuButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    //SpeciesButton::Render( realX, realY, highlighted, clicked );
    if( !m_iconName ) return;
    SpeciesWindow *parent = (SpeciesWindow *)m_parent;

    realX += 150;
    UpdateButtonHighlight();


    glColor4f( 1.0f, 1.0f, 1.0f, 0.0f );
    g_gameFont.SetRenderOutline(true);
    g_gameFont.DrawText2DCentre( realX, realY, m_fontSize, m_iconName );

    glColor4f( 1.3f, 1.0f, 1.3f, 1.0f );

    if( !m_mouseHighlightMode )
	{
		highlighted = false;
	}

    if( parent->m_buttonOrder[parent->m_currentButton] == this )
	{
		highlighted = true;
	}

    if( highlighted  )
    {
        glColor4f( 1.0, 0.3f, 0.3, 1.0f );
    }

    g_gameFont.SetRenderOutline(false);
    g_gameFont.DrawText2DCentre( realX, realY, m_fontSize, m_iconName );



    /*glEnable        ( GL_TEXTURE_2D );
    glBindTexture   ( GL_TEXTURE_2D, g_app->m_resource->GetTexture( m_iconName ) );
    glBlendFunc     ( GL_SRC_ALPHA, GL_ONE );
    //glTexParameterf ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    //glTexParameterf ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

    glColor4f( 0.3f, 1.0f, 0.3f, 1.0f );

    if( parent->m_buttonOrder[parent->m_currentButton] == this )
	{
		highlighted = true;
	}

    if( highlighted  )
    {
        glColor4f( 1.0, 0.3f, 0.3, 1.0f );
    }

    glBegin( GL_QUADS );
        glTexCoord2f( 0.0f, 1.0f );     glVertex2f( realX, realY);
        glTexCoord2f( 1.0f, 1.0f );     glVertex2f( realX+ m_w, realY );
        glTexCoord2f( 1.0f, 0.0f );     glVertex2f( realX+ m_w, realY + m_h );
        glTexCoord2f( 0.0f, 0.0f );     glVertex2f( realX, realY + m_h );
    glEnd();

    glDisable       ( GL_TEXTURE_2D );*/
}

class BackPageButton : public GameMenuButton
{
public:
    int m_pageId;

    BackPageButton( int _page )
    //:   GameMenuButton( "Icons/menu_back.bmp" )
    :   GameMenuButton( "Back" )
    {
        m_pageId = _page;
    }

    void MouseUp()
    {
        ((GameMenuWindow *) m_parent)->m_newPage = m_pageId;
    }
};

class QuitButton : public GameMenuButton
{
public:
    QuitButton()
    //:   GameMenuButton( "Icons/menu_quit.bmp" )
    :   GameMenuButton( "Quit" )
    {
    }

    void MouseUp()
    {
        g_app->m_requestQuit = true;
    }
};

class PrologueButton : public GameMenuButton
{
public:
    PrologueButton( char const *_iconName )
    :   GameMenuButton( _iconName )
    {
    }

    void MouseUp()
    {
        g_app->LoadPrologue();
    }
};

class CampaignButton : public GameMenuButton
{
public:
    CampaignButton( char const *_iconName )
    :   GameMenuButton( _iconName )
    {
    }

    void MouseUp()
    {
        g_app->LoadCampaign();
    }
};

class SpeciesModeButton : public GameMenuButton
{
public:
    SpeciesModeButton( char const *_iconName )
    :   GameMenuButton( _iconName )
    {
    }

    void MouseUp()
    {
        ((GameMenuWindow *) m_parent)->m_newPage = GameMenuWindow::PageDarwinia;
        /*if( g_app->m_multiwinia )
        {
            delete g_app->m_multiwinia;
            g_app->m_multiwinia = NULL;
        }*/
    }
};
/*
class GameTypeButton : public GameMenuButton
{
public:
    int m_gameType;

    GameTypeButton( char *_iconName, int _gameType )
    :   GameMenuButton( _iconName ),
        m_gameType(_gameType)
    {
    }

    void MouseUp()
    {
        ((GameMenuWindow *) m_parent)->m_newPage = GameMenuWindow::PageGameSetup;
        ((GameMenuWindow *) m_parent)->m_gameType = m_gameType;
    }
};

class MultiwiniaModeButton : public GameMenuButton
{
public:
    MultiwiniaModeButton( char *_iconName )
    :   GameMenuButton( _iconName )
    {
    }

    void MouseUp()
    {
        ((GameMenuWindow *) m_parent)->m_newPage = GameMenuWindow::PageMultiwinia;
        if( !g_app->m_multiwinia )
        {
            g_app->m_multiwinia = new Multiwinia();
        }

    }
};

class PlayGameButton : public GameMenuButton
{
public:
    PlayGameButton()
    : GameMenuButton( "Play" )
    {
    }

    void MouseUp()
    {
        GameMenuWindow *parent = (GameMenuWindow *)m_parent;

        if( g_app->m_gameMenu->m_maps[parent->m_gameType].ValidIndex(parent->m_requestedMapId ) )
        {
            strcpy( g_app->m_requestedMap, g_app->m_gameMenu->m_maps[parent->m_gameType][parent->m_requestedMapId] );
            strcpy( g_app->m_requestedMission, "null" );
        }

        g_app->m_requestToggleEditing = false;
        g_app->m_requestedLocationId = 999;

        g_app->m_multiwinia->SetGameResearch( parent->m_researchLevel );
        g_app->m_multiwinia->SetGameOptions( parent->m_gameType, parent->m_params );

        g_app->m_atMainMenu = false;
        g_app->m_gameMode = App::GameModeMultiwinia;
    }
};

class ResearchModeButton : public GameMenuButton
{
public:
    ResearchModeButton()
    : GameMenuButton( "Research" )
    {
    }

    void MouseUp()
    {
        ((GameMenuWindow *) m_parent)->m_newPage = GameMenuWindow::PageResearch;
    }
};
*/

// *************************
// Game Menu Class
// *************************

GameMenu::GameMenu()
:   m_menuCreated(false)
{
    m_internet = new GlobalInternet();
}

void GameMenu::Render()
{
    if( m_internet )
    {
        m_internet->Render();
    }
}

void GameMenu::CreateMenu()
{
    g_app->m_renderer->StartFadeIn(0.25f);
    // close all currently open windows
    LList<EclWindow *> *windows = EclGetWindows();
	while (windows->Size() > 0) {
		EclWindow *w = windows->GetData(0);
		EclRemoveWindow(w->m_name);
	}

    // create the actual menu window
    EclRegisterWindow( new GameMenuWindow() );

    // set the camera to a position with a good view of the internet
    g_app->m_camera->RequestMode(Camera::ModeMainMenu);
    g_app->m_camera->SetDebugMode(Camera::DebugModeNever);
    g_app->m_camera->SetTarget(Vector3(-900000, 3000000, 397000), Vector3(0,0.5f,-1));
    g_app->m_camera->CutToTarget();

    /*if( g_app->m_multiwinia )
    {
        delete g_app->m_multiwinia;
        g_app->m_multiwinia = NULL;
    }*/

    g_app->m_gameMode = App::GameModeNone;

    m_menuCreated = true;
}

void GameMenu::DestroyMenu()
{
    m_menuCreated = false;
    EclRemoveWindow("GameMenu");
    g_app->m_renderer->StartFadeOut();
}
/*
void GameMenu::CreateMapList()
{
    LList<char *> *levels = g_app->m_resource->ListResources( "Levels/", "mp_*", false );

    for( int i = 0; i < levels->Size(); ++i )
    {
        char filename[512];
        sprintf( filename, "Levels/%s", levels->GetData(i) );
        TextReader *file = g_app->m_resource->GetTextReader( filename );
        if( file && file->IsOpen() )
        {
            while( file->ReadLine() )
            {
                if( !file->TokenAvailable() ) continue;

                char *gameTypes = file->GetNextToken();
                if( strcmp( gameTypes, "GameTypes" ) == 0 )
                {
                    while( file->TokenAvailable() )
                    {
                        char *type = file->GetNextToken();
                        for( int j = 0; j < Multiwinia::s_gameBlueprints.Size(); ++j )
                        {
                            if( strcmp( type, Multiwinia::s_gameBlueprints[j]->m_name ) == 0 )
                            {
                                m_maps[j].PutData( levels->GetData(i) );
                            }
                        }
                    }
                    break;
                }
            }
            delete file;
        }
    }

    delete levels;
}*/

// *************************
// GameMenuWindow Class
// *************************


GameMenuWindow::GameMenuWindow()
:   SpeciesWindow("GameMenu"),
    m_currentPage(-1),
    m_newPage(PageDarwinia)
{
    int w = g_app->m_renderer->ScreenW();
    int h = g_app->m_renderer->ScreenH();
    SetPosition( 5, 5 );
    SetSize( w - 10, h - 10 );
    m_resizable = false;
    SetMovable( false );
}

void GameMenuWindow::Create()
{
}

void GameMenuWindow::Update()
{
    SpeciesWindow::Update();
    if( m_currentPage != m_newPage )
    {
        SetupNewPage( m_newPage );

        int w = g_app->m_renderer->ScreenW();
        int h = g_app->m_renderer->ScreenH();
        SetPosition( 5, 5 );
        SetSize( w - 10, h - 10 );
    }
}

void GameMenuWindow::Render( bool _hasFocus )
{
    //SpeciesWindow::Render( _hasFocus );
    // render nothing but the buttons
    EclWindow::Render( _hasFocus );

    int w = g_app->m_renderer->ScreenW();
    int h = g_app->m_renderer->ScreenH();

    glColor4f( 1.0f, 1.0f, 1.0f, 0.0f );
    g_gameFont.SetRenderOutline(true);
    g_gameFont.DrawText2DCentre( w/2, 30, 80.0f, "DARWINIA" );

    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    g_gameFont.SetRenderOutline(false);
    g_gameFont.DrawText2DCentre( w/2, 30, 80.0f, "DARWINIA" );
}


void GameMenuWindow::SetupNewPage( int _page )
{
    Remove();

    switch( _page )
    {
        case PageMain:      SetupMainPage();                    break;
        case PageDarwinia:  SetupDarwiniaPage();                break;
        /*case PageMultiwinia:SetupMultiwiniaPage();              break;
        case PageGameSetup: SetupMultiplayerPage( m_gameType ); break;
        case PageResearch:  SetupResearchPage();                break;*/
    }

    m_currentPage = _page;
}

void GameMenuWindow::SetupMainPage()
{
    int x, y, gap;
    GetDefaultPositions( &x, &y, &gap );
    int h = 60;
    int w = 300;

    //SpeciesModeButton *dmb = new SpeciesModeButton( "Icons/menu_darwinia.bmp" );
    SpeciesModeButton *dmb = new SpeciesModeButton( "Darwinia" );
    dmb->SetShortProperties("darwinia", x, y, w, h );
    RegisterButton( dmb );
    m_buttonOrder.PutData( dmb );

    QuitButton *quit = new QuitButton();
    quit->SetShortProperties( "quit", x, y+=gap, w, h );
    RegisterButton( quit );
    m_buttonOrder.PutData( quit );
}

void GameMenuWindow::SetupDarwiniaPage()
{
    int x, y, gap;
    GetDefaultPositions( &x, &y, &gap );
    int h = 60;
    int w = 300;

//    PrologueButton *pb = new PrologueButton( "Icons/menu_prologue.bmp" );
    PrologueButton *pb = new PrologueButton( "Prologue" );
    pb->SetShortProperties("prologue", x, y, w, h );
    RegisterButton( pb );
    m_buttonOrder.PutData( pb );

//    CampaignButton *cb = new CampaignButton( "Icons/menu_campaign.bmp" );
    CampaignButton *cb = new CampaignButton( "Campaign" );
    cb->SetShortProperties( "campaign", x, y+=gap, w, h );
    RegisterButton( cb );
    m_buttonOrder.PutData( cb);

    QuitButton *quit = new QuitButton();
    quit->SetShortProperties( "quit", x, y+=gap, w, h );
    RegisterButton( quit );
    m_buttonOrder.PutData( quit );
}

void GameMenuWindow::GetDefaultPositions(int *_x, int *_y, int *_gap)
{
    float w = g_app->m_renderer->ScreenW();
    float h = g_app->m_renderer->ScreenH();

    *_x = (w / 2) - 150;
    switch( m_newPage )
    {
        case PageMain:
        case PageDarwinia:      *_y = float((h / 864.0f ) * 200.0f); *_gap = *_y;            break;
        case PageMultiwinia:    *_y = float((h / 864.0f ) * 200.0f); *_gap = *_y / 1.5f;     break;
        case PageGameSetup:
        case PageResearch:      *_y = float((h / 864.0f ) * 70.0f); *_gap = (h / 864 ) * 60; break;
    }

    //*_x = min( *_x, 200 );
}
