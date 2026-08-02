#include "pch.h"
#include "FilesysUtils.h"
#include "TextRenderer.h"
#include "LanguageTable.h"

#include "UserProfileWindow.h"

#include "Server.h"

#include "InputField.h"
#include "Renderer.h"
#include "WorldPointers.h"
#include "AppState.h"
#include "AppCommands.h"


class LoadUserProfileButton : public SpeciesButton
{
public:
    char *m_profileName;
    void MouseUp()
    {
        g_appCommands->SetProfileName( m_profileName );
        g_appCommands->LoadProfile();
        EclRemoveWindow( m_parent->m_name );
        EclRemoveWindow( LANGUAGEPHRASE("dialog_mainmenu") );
    }
};


class NewProfileWindowButton : public SpeciesButton
{
public:
    void MouseUp()
    {
        EclRegisterWindow( new NewUserProfileWindow(), m_parent );
    }
};


UserProfileWindow::UserProfileWindow()
:   SpeciesWindow(LANGUAGEPHRASE("dialog_profile"))
{
}


void UserProfileWindow::Render( bool hasFocus )
{
    SpeciesWindow::Render( hasFocus );

    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    g_editorFont.DrawText2DCentre( m_x+m_w/2, m_y+GetMenuSize(30), GetMenuSize(12), LANGUAGEPHRASE("dialog_currentprofilename") );
    g_editorFont.DrawText2DCentre( m_x+m_w/2, m_y+GetMenuSize(45), GetMenuSize(16), g_userProfileName );
}


void UserProfileWindow::Create()
{
    char profileDir[256];
    sprintf(profileDir, "%susers/*.*", g_appCommands->ProfileDirectory());
    LList<char *> *profileList = ListSubDirectoryNames( profileDir );
    int numProfiles = profileList->Size();

    int windowH = 150 + numProfiles * 30;
    SetMenuSize( 300, windowH);
	SetPosition( g_renderer->ScreenW()/2 - m_w/2,
                 g_renderer->ScreenH()/2 - m_h/2 );

    SpeciesWindow::Create();

    //
    // New profile

    int y = GetMenuSize(50);
    int h = GetMenuSize(30);

    int invertY = y+GetMenuSize(20);

    //
    // Inverted box

    if( numProfiles > 0 )
    {
        InvertedBox *box = new InvertedBox();
        box->SetShortProperties( "Box", 10, invertY, m_w-20, m_h - invertY - GetMenuSize(60) );
        RegisterButton( box );
    }

    //
    // Load existing profile button

    for( int i = 0; i < profileList->Size(); ++i )
    {
        char *profileName = profileList->GetData(i);
        char caption[256];
        sprintf( caption, "%s: '%s'", LANGUAGEPHRASE("dialog_loadprofile"), profileName );
        LoadUserProfileButton *button = new LoadUserProfileButton();
        button->SetShortProperties( caption, 20, y+=h, m_w-40, GetMenuSize(20) );
        button->m_profileName = profileName;
        button->m_fontSize = GetMenuSize(11);
        button->m_centered = true;
        RegisterButton( button );
        m_buttonOrder.PutData( button );
    }
    delete profileList;


    NewProfileWindowButton *newProfile = new NewProfileWindowButton();
    newProfile->SetShortProperties( LANGUAGEPHRASE("dialog_newprofile"), 10, m_h - GetMenuSize(55), m_w - 20, GetMenuSize(20) );
    newProfile->m_fontSize = GetMenuSize(13);
    newProfile->m_centered = true;
    RegisterButton( newProfile );
    m_buttonOrder.PutData( newProfile );

    CloseButton *cancel = new CloseButton();
    cancel->SetShortProperties( LANGUAGEPHRASE("dialog_close"),  10, m_h - GetMenuSize(30), m_w - 20, GetMenuSize(20) );
    cancel->m_fontSize = GetMenuSize(13);
    cancel->m_centered = true;
    RegisterButton( cancel );
	m_buttonOrder.PutData( cancel );


}



// ============================================================================


class NewProfileButton : public SpeciesButton
{
    void MouseUp()
    {
        NewUserProfileWindow *parent = (NewUserProfileWindow *) m_parent;
        g_appCommands->SetProfileName( parent->s_profileName );
        g_appCommands->LoadProfile();
        EclRemoveWindow( m_parent->m_name );
        EclRemoveWindow( LANGUAGEPHRASE("dialog_newprofile") );
        EclRemoveWindow( LANGUAGEPHRASE("dialog_mainmenu") );
    }
};

char NewUserProfileWindow::s_profileName[256] = "NewUser";


NewUserProfileWindow::NewUserProfileWindow()
:   SpeciesWindow( LANGUAGEPHRASE("dialog_newprofile") )
{
}


void NewUserProfileWindow::Create()
{
    SetMenuSize( 300, 110 );
	SetPosition( g_renderer->ScreenW()/2 - m_w/2,
                 g_renderer->ScreenH()/2 - m_h/2 );

    SpeciesWindow::Create();

    InvertedBox *box = new InvertedBox();
    box->SetShortProperties( "box", 10, GetMenuSize(30), m_w-20, GetMenuSize(40) );
    RegisterButton( box );

    CreateValueControl( LANGUAGEPHRASE("dialog_name"), InputField::TypeString, s_profileName, GetMenuSize(40), 0, 0, 0, nullptr, 20, m_w-40 );

	int y = m_h-GetMenuSize(30);

    CloseButton *close = new CloseButton();
    close->SetShortProperties( LANGUAGEPHRASE("dialog_cancel"), 10, y, m_w/2-15, GetMenuSize(20) );
	close->m_fontSize = GetMenuSize(12);
    RegisterButton( close );

    NewProfileButton *newProfile = new NewProfileButton();
    newProfile->SetShortProperties( LANGUAGEPHRASE("dialog_create"), close->m_x+close->m_w+10, y, m_w/2-15, GetMenuSize(20) );
	newProfile->m_fontSize = GetMenuSize(12);
    RegisterButton( newProfile );
}
