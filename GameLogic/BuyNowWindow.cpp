#include "pch.h"
#include "TextRenderer.h"
#include "WindowManager.h"
#include "Resource.h"
#include "LanguageTable.h"

#include "BuyNowWindow.h"
#include "Preferences.h"
#include "PreferenceNames.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  class BuyNowButton : public SpeciesButton
  {
      void MouseUp()
      {
        g_requestQuit = true;
        EclRemoveWindow(m_parent->m_name.c_str());
      }
  };


  BuyNowWindow::BuyNowWindow()
    : SpeciesWindow(LANGUAGEPHRASE("dialog_buydarwinia"))
  {
    int screenW = g_renderer->ScreenW();
    int screenH = g_renderer->ScreenH();

    SetSize( 370, 150 );
    SetPosition( screenW/2.0f - m_w/2.0f,
                 screenH/2.0f - m_h/2.0f );
  }

void BuyNowWindow::Create()
{
	SpeciesWindow::Create();

	int y = m_h;
	int h = 30;

    SpeciesButton *close = new CloseButton();
    close->SetShortProperties( LANGUAGEPHRASE("dialog_later"), 10, y-=h, m_w-20, 20 );
    close->m_fontSize = 13;
    close->m_centered = true;
    RegisterButton( close );

    SpeciesButton *buy = new BuyNowButton();
    buy->SetShortProperties( LANGUAGEPHRASE("dialog_buynow"), 10, y-=h, m_w-20, 20 );
    buy->m_fontSize = 13;
    buy->m_centered = true;
    RegisterButton( buy );
}

void BuyNowWindow::Render(bool _hasFocus)
{
	SpeciesWindow::Render(_hasFocus);

    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	int y = m_y + 25;
	int h = 18;

	const char *line[] = {
		LANGUAGEPHRASE("dialog_buynow1"),
		LANGUAGEPHRASE("dialog_buynow2"),
		nullptr
	};

	for (int i = 0; line[i]; i++)
		g_gameFont.DrawText2DCentre( m_x+m_w/2, y+=h, 13, line[i] );

}
} // namespace Species
