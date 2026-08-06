#include "pch.h"

#include "Win32EventHandler.h"

#include "InputDriverWin32.h"
#include "WindowManager.h"
#include "WindowManagerWin32.h"
#include "Debug.h"

#include "AppState.h"


namespace Neuron
{
  W32EventHandler* g_eventHandler = nullptr;

  // The externs for the driver's g_keys and g_keyDeltas are GONE, and so are
  // the globals themselves as of T5 — the driver holds one InputFrameState and
  // nothing outside it can reach the arrays at all. This file reached into them
  // to clear a stuck ALT on focus loss; that job belongs to the driver, through
  // OnFocusLost, which now enqueues a FocusLost event so the release edges are
  // produced in order with every other message. What is left here is the focus
  // flag itself.
  bool g_windowHasFocus = true;


  LRESULT CALLBACK W32EventHandler::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
    // Focus is now told to us rather than asked for. This used to call
    // GetForegroundWindow on EVERY message — thousands a second, to answer a
    // question that changes a handful of times a session — and it inferred the
    // transition by comparing against a static from the previous message.
    case WM_ACTIVATE:
      g_windowHasFocus = LOWORD(wParam) != WA_INACTIVE;

      if (!g_windowHasFocus && g_win32InputDriver)
      {
        // Everything held goes, not just ALT. Windows sends no release for
        // any of it, so whatever was down at this moment would otherwise stay
        // down forever as far as the game is concerned.
        g_win32InputDriver->OnFocusLost();
      }

      return -1;

    case WM_SIZING:
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
    case WM_DESTROY:
    case WM_CREATE:
    case WM_COMMAND:
    case WM_NCHITTEST: // Mouse move or button
      return -1;

    case WM_SETCURSOR:
    {
      POINT pt;
      GetCursorPos(&pt);
      LPARAM lparam = (unsigned long)(pt.x & 0xFFFF) | (unsigned long)((pt.y & 0xFFFF) << 16);
      if (DefWindowProc(hWnd, WM_NCHITTEST, 0, lparam) == HTCLIENT)
        SetCursor(nullptr);
      else
        return -1;
    }

    case WM_MOVE:
      g_windowManager->WindowMoved();
      return -1;

    case WM_CANCELMODE:
      return 0;

    case WM_CLOSE:
      g_requestQuit = true;
      return 0;

    case WM_INPUTLANGCHANGE:
      DebugTrace("Input language change: w = {}, l = {}\n", wParam, lParam);
      // Might want to reload key bindings and translations here if we can be bothered.
      return 0;
    }

    // STRAIGHT TO THE DRIVER. This used to walk a vector of W32EventProcessors
    // and stop at the first that claimed the message; there has only ever been
    // one processor in it, and the abstraction it was reached through has been
    // deleted along with the vector.
    if (!g_win32InputDriver)
      return -1;

    return g_win32InputDriver->WndProc(hWnd, message, wParam, lParam);
  }


  bool W32EventHandler::WindowHasFocus() const { return g_windowHasFocus; }
} // namespace Neuron
