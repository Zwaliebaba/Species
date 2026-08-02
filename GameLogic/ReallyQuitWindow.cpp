#include "pch.h"

#include "Renderer.h"
#include "ReallyQuitWindow.h"
#include "WorldPointers.h"

ReallyQuitWindow::ReallyQuitWindow()
	:   SpeciesWindow(REALLYQUIT_WINDOWNAME)
{
	m_w = 160;
	m_h = 90;
	m_x = g_renderer->ScreenW()/2 - m_w/2;
	m_y = g_renderer->ScreenH()/2 - m_h/2;
}

void ReallyQuitWindow::Create()
{
	SpeciesWindow::Create();

	int y = 0, h = 30;

    SpeciesButton *exit = new GameExitButton();
    exit->SetShortProperties( "Leave Darwinia", 10, y+=h, m_w-20, 20 );
    exit->m_fontSize = 13;
    exit->m_centered = true;
    RegisterButton( exit );

    SpeciesButton *close = new CloseButton();
    close->SetShortProperties( "No. Play On!", 10, y+=h, m_w-20, 20 );
    close->m_fontSize = 13;
    close->m_centered = true;
    RegisterButton( close );
}

