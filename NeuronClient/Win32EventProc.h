#pragma once

#include <windows.h>


class W32EventProcessor {

public:
	// This is a callback for Windows events
	// Returns 0 if the event is handled here, -1 otherwise
	virtual LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) = 0;

};


