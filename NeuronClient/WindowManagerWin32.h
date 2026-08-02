#pragma once

class WindowManagerWin32
{
public:
	HWND		m_hWnd;
	HDC			m_hDC;
	HGLRC		m_hRC;

	WindowManagerWin32()
	:	m_hWnd(nullptr),
		m_hDC(nullptr),
		m_hRC(nullptr)
	{
	}
};

