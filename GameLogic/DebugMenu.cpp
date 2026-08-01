#include "pch.h"
#include "DebugUtils.h"
//#include "Input.h"
#include "TextRenderer.h"
#include "Preferences.h"
#include "WindowManager.h"
#include "LanguageTable.h"

#include "DebugMenu.h"
#include "NetworkWindow.h"
#include "PrefsScreenWindow.h"
#include "PrefsGraphicsWindow.h"
#include "PrefsSoundWindow.h"
#include "ProfileWindow.h"
#include "CheatWindow.h"
#include "UserProfileWindow.h"
#include "ReallyQuitWindow.h"

#include "App.h"
#include "Camera.h"
#include "Renderer.h"
#include "UserInput.h"

// ****************************************************************************
// Menu Buttons
// ****************************************************************************

#ifdef PROFILER_ENABLED
class ProfileButton : public DarwiniaButton
{
public:
	void MouseUp()
    {
		DebugKeyBindings::ProfileButton();
    }
};
#endif // PROFILER_ENABLED


class NetworkButton: public DarwiniaButton
{
public:
    void MouseUp()
    {
		DebugKeyBindings::NetworkButton();
    }
};


#ifdef LOCATION_EDITOR
class EditorButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
		DebugKeyBindings::EditorButton();
    }
};
#endif // LOCATION_EDITOR


class DebugCameraButton: public DarwiniaButton
{
public:
	void MouseUp()
	{
		DebugKeyBindings::DebugCameraButton();
	}
};


class FPSButton: public DarwiniaButton
{
public:
	void MouseUp()
	{
		DebugKeyBindings::FPSButton();
	}
};


class PrefsScreenButton: public DarwiniaButton
{
public:
	void MouseUp()
	{
		if (!EclGetWindow(LANGUAGEPHRASE("dialog_screenoptions")))
		{
			EclRegisterWindow(new PrefsScreenWindow());
		}
	}
};


class PrefsGfxDetailButton: public DarwiniaButton
{
public:
	void MouseUp()
	{
		if (!EclGetWindow(LANGUAGEPHRASE("dialog_graphicsoptions")))
		{
			EclRegisterWindow(new PrefsGraphicsWindow());
		}
    }
};


class PrefsSoundButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        if(!EclGetWindow(LANGUAGEPHRASE("dialog_soundoptions")))
        {
            EclRegisterWindow(new PrefsSoundWindow());
        }
    };
};


#ifdef CHEATMENU_ENABLED
class CheatButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        DebugKeyBindings::CheatButton();
    }
};
#endif


// ****************************************************************************
// Class DebugMenu
// ****************************************************************************

DebugMenu::DebugMenu( char *name )
:   DarwiniaWindow( name )
{
	m_x = 10;
	m_y = 20;
	m_w = 170;
	m_h = 75;
}


void DebugMenu::Advance()
{
}


void DebugMenu::Create()
{
	DarwiniaWindow::Create();

    int pitch = 18;
	int y = 5;

	DarwiniaButton *button;

#ifdef PROFILER_ENABLED
	button = new ProfileButton();
    button->SetShortProperties( "Profile (F6)", 10, y += pitch, m_w - 20 );
    RegisterButton( button );
#endif // PROFILER_ENABLED

    button = new NetworkButton();
    button->SetShortProperties( "Network Stats", 10, y += pitch, m_w - 20 );
    RegisterButton( button );

	button = new FPSButton();
	button->SetShortProperties("Display FPS (F5)", 10, y += pitch, m_w - 20 );
	RegisterButton( button );

	y += pitch / 2.0f;

	y += pitch / 2.0f;

	button = new DebugCameraButton();
	button->SetShortProperties("Dbg Cam (F2)", 10, y += pitch, m_w - 20 );
	RegisterButton( button );

	y += pitch / 2.0f;

    bool modsEnabled = g_prefsManager->GetInt( "ModSystemEnabled", 0 ) != 0;

#ifdef LOCATION_EDITOR
    if( modsEnabled )
    {
        button = new EditorButton();
	    button->SetShortProperties("Toggle Editor (F3)", 10, y += pitch, m_w - 20);
	    RegisterButton(button);
    }
#endif // LOCATION_EDITOR

#ifdef CHEATMENU_ENABLED
    button = new CheatButton();
    button->SetShortProperties("Cheat Menu (F4)", 10, y += pitch, m_w - 20 );
    RegisterButton( button );
#endif


	y += pitch / 2.0f;


}


void DebugMenu::Render(bool hasFocus)
{
	Advance();

	DarwiniaWindow::Render(hasFocus);

	EclButton *camDbgButton = GetButton("Dbg Cam (F2)");
	DarwiniaDebugAssert(camDbgButton);
	int y = m_y + camDbgButton->m_y + 11;

	switch (g_app->m_camera->GetDebugMode())
	{
		case Camera::DebugModeAlways:
			g_editorFont.DrawText2D(m_x + m_w - 47, y, 10, "Always");
			break;
		case Camera::DebugModeAuto:
			g_editorFont.DrawText2D(m_x + m_w - 47, y, 10, "Auto");
			break;
		case Camera::DebugModeNever:
			g_editorFont.DrawText2D(m_x + m_w - 47, y, 10, "Never");
			break;
	}
}


// ****************************************************************************
// Class DebugKeyBindings
// ****************************************************************************

void DebugKeyBindings::DebugMenu()
{
	char *debugMenuWindowName = LANGUAGEPHRASE("dialog_toolsmenu");
	if (EclGetWindow(debugMenuWindowName))
		EclRemoveWindow(debugMenuWindowName);
	else
		EclRegisterWindow(new ::DebugMenu(debugMenuWindowName));
}

#ifdef PROFILER_ENABLED
void DebugKeyBindings::ProfileButton()
{
    if( EclGetWindow("Profiler") )
	{
		EclRemoveWindow("Profiler");
	}
	else
    {
        ProfileWindow *pw = new ProfileWindow("Profiler");
        pw->m_w = 570;
        pw->m_h = 450;
        pw->m_x = g_app->m_renderer->ScreenW() - pw->m_w - 20;
        pw->m_y = 30;
        EclRegisterWindow(pw);
    }
}
#endif


void DebugKeyBindings::NetworkButton()
{
    if (!EclGetWindow("Network Stats") )
    {
        NetworkWindow *nw = new NetworkWindow("Network Stats");
        nw->m_w = 200;
        nw->m_h = 200;
        nw->m_x = 10;
        nw->m_y = g_app->m_renderer->ScreenH() - nw->m_h;
        EclRegisterWindow(nw);
    }
}


#ifdef LOCATION_EDITOR
void DebugKeyBindings::EditorButton()
{
	g_app->m_requestToggleEditing = true;
}
#endif // LOCATION_EDITOR


void DebugKeyBindings::DebugCameraButton()
{
	g_app->m_camera->SetNextDebugMode();
}

void DebugKeyBindings::FPSButton()
{
	g_app->m_renderer->m_displayFPS = !g_app->m_renderer->m_displayFPS;
}


#ifdef CHEATMENU_ENABLED
void DebugKeyBindings::CheatButton()
{
    if( !EclGetWindow("Cheat Window") )
    {
        CheatWindow *window = new CheatWindow("Cheat Window" );
        window->m_w = 200;
        window->m_h = 200;
        window->m_x = 250;
        window->m_y = 50;
        EclRegisterWindow( window );
    }
}
#endif


void DebugKeyBindings::ReallyQuitButton()
{
	// Bring up a really quit window
	if (!EclGetWindow(REALLYQUIT_WINDOWNAME))
		EclRegisterWindow( new ReallyQuitWindow() );
}

void DebugKeyBindings::ToggleFullscreenButton()
{
	bool switchingToWindowed;
	SetWindowed(!g_windowManager->Windowed(), true, switchingToWindowed);
}
