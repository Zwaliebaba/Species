#include "pch.h"
#include "TextRenderer.h"
#include "LanguageTable.h"

#include "SoundSystem.h"

#include "SaveOnQuitWindow.h"

#include "App.h"
#include "Renderer.h"


class YesButton : public SpeciesButton
{
    void MouseUp()
    {
		g_soundSystem->SaveBlueprints();
		g_app->m_requestQuit = true;
    }
};


class NoButton : public SpeciesButton
{
    void MouseUp()
    {
		g_app->m_requestQuit = true;
		g_soundSystem->m_quitWithoutSave = true;
    }
};


class CancelButton : public SpeciesButton
{
    void MouseUp()
    {
		EclRemoveWindow(LANGUAGEPHRASE("editor_savesettings"));
    }
};


SaveOnQuitWindow::SaveOnQuitWindow( char const *_name )
:   SpeciesWindow( _name )
{
	m_w = 200;
	m_h = 100;
	m_x = g_app->m_renderer->ScreenW()/2 - m_w/2;
	m_y = g_app->m_renderer->ScreenH()/2 - m_h/2;
}


void SaveOnQuitWindow::Create()
{
	SpeciesWindow::Create();

    SpeciesButton *button;
	int width = 55;
	int pitch = width + 8;
	int x = 1 - width;
	int y = m_h - 25;

	button = new YesButton();
    button->SetShortProperties( "Yes", x += pitch, y, width);
    RegisterButton( button );

	button = new NoButton();
    button->SetShortProperties( "No", x += pitch, y, width );
    RegisterButton( button );

	button = new CancelButton();
    button->SetShortProperties( "Cancel", x += pitch, y, width );
    RegisterButton( button );
}


void SaveOnQuitWindow::Render(bool _hasFocus)
{
	SpeciesWindow::Render(_hasFocus);
	g_editorFont.DrawText2D(m_x + 15, m_y + 30, DEF_FONT_SIZE * 4, "!");
	g_editorFont.DrawText2D(m_x + 55, m_y + 38, DEF_FONT_SIZE, "Save changes to");
	g_editorFont.DrawText2D(m_x + 55, m_y + 52, DEF_FONT_SIZE, "sounds.txt?");
}
