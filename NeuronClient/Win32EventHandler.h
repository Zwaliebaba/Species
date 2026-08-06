#pragma once

//*****************************************************************************
// The window procedure's one stop before the input driver
//*****************************************************************************
//
// THREE TYPES USED TO STAND HERE, each with exactly one implementation:
// EventHandler (one pure virtual, WindowHasFocus), W32EventProcessor (a WndProc
// and a focus-lost hook) and W32EventHandler, which inherited from both and
// kept a VECTOR of processors so that it could fan one window procedure out
// to — one consumer. `g_eventHandler` was an EventHandler*, so reaching the
// only implementation meant a dynamic_cast, in a function called on every
// message.
//
// With events in a queue the fan-out is the router's job, so all of that is one
// concrete class: the window procedure handles what belongs to the WINDOW —
// focus, moves, close — and hands everything else to the input driver.

#include <windows.h>


namespace Neuron
{
  class W32EventHandler
  {
    public:
      // Called by the WindowManager's window procedure.
      // Returns 0 if the event was handled, -1 to pass it to DefWindowProc.
      LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

      // Answered from WM_ACTIVATE. Event-derived since T4, when it stopped
      // being a GetForegroundWindow call made on every single message.
      bool WindowHasFocus() const;
  };


  // THE ONE HANDLER, typed as what it is. It was an EventHandler* and every use
  // went through a dynamic_cast to get back to this type.
  extern W32EventHandler* g_eventHandler;
} // namespace Neuron
