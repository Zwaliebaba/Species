#include "pch.h"

#include <iostream>

#include "Win32EventHandler.h"

#include "WindowManager.h"
#include "WindowManagerWin32.h"
#include "Debug.h"

#include "AppState.h"

using std::cerr;


namespace Neuron
{
  typedef std::vector<W32EventProcessor*> ProcList;
  typedef ProcList::iterator ProcIt;

  // The externs for the driver's g_keys and g_keyDeltas are GONE, and so are
  // the globals themselves as of T5 — the driver holds one InputFrameState and
  // nothing outside it can reach the arrays at all. This file reached into them
  // to clear a stuck ALT on focus loss; that job belongs to the driver, through
  // OnFocusLost, which now enqueues a FocusLost event so the release edges are
  // produced in order with every other message. What is left here is the focus
  // flag itself.
  bool g_windowHasFocus = true;


  W32EventHandler::W32EventHandler()
    : w32eventprocs()
  {
  }


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

      if (!g_windowHasFocus)
      {
        // Everything held goes, not just ALT. Windows sends no release for
        // any of it, so whatever was down at this moment would otherwise stay
        // down forever as far as the game is concerned.
        for (W32EventProcessor* processor : w32eventprocs)
          processor->OnFocusLost();
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

    int ans = -1;
    for (ProcIt i = w32eventprocs.begin(); i != w32eventprocs.end(); ++i)
    {
      ans = (*i)->WndProc(hWnd, message, wParam, lParam);
      if (ans != -1)
        break;
    }
    return ans;
  }


  void W32EventHandler::AddEventProcessor(W32EventProcessor* _driver)
  {
    if (_driver)
      w32eventprocs.push_back(_driver);
    else
      cerr << "AddEventProcessor: _driver is NULL\n";
  }


  void W32EventHandler::RemoveEventProcessor(W32EventProcessor* _driver)
  {
    for (ProcIt i = w32eventprocs.begin(); i != w32eventprocs.end(); ++i)
      if (_driver == *i)
        w32eventprocs.erase(i);
  }


  bool W32EventHandler::WindowHasFocus() { return g_windowHasFocus; }


  W32EventHandler* getW32EventHandler()
  {
    EventHandler* handler = g_eventHandler;
    return dynamic_cast<W32EventHandler*>(handler);
  }
} // namespace Neuron
