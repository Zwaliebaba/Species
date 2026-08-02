#include "pch.h"
#include "UpdateAvailableWindow.h"
#ifdef AMBROSIA_REGISTRATION
#include "ambrosia.h"
#endif
#include "WindowManager.h"

class GetItNowButton : public SpeciesButton {
public:
    void MouseUp()
    {
#ifdef AMBROSIA_REGISTRATION
		g_windowManager->OpenWebsite("http://www.AmbrosiaSW.com/games/darwinia/");
#endif
		EclRemoveWindow( m_parent->m_name );
    }
};

static char *buf = NULL;

UpdateAvailableWindow::UpdateAvailableWindow( const char *newVersion, const char *changeLog )
	: MessageDialog("New version available",
				(sprintf(
					buf = new char [512 + strlen(newVersion) + strlen(changeLog)],
					"There is a new version of Darwinia available (%s) at\n"
					"http://www.ambrosiasw.com/games/darwinia/\n"
					"\n"
					"%s",
					newVersion, changeLog), buf))
{
	m_h += 30;
}

void UpdateAvailableWindow::Create()
{
	MessageDialog::Create();

	int const buttonWidth = 80;
	int const buttonHeight = 18;
	SpeciesButton *button = new GetItNowButton();
    button->SetShortProperties( "Get It Now!", (m_w - buttonWidth)/2, m_h - 60, buttonWidth, buttonHeight );
    RegisterButton( button );
}

