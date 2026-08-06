#pragma once

#include <windows.h>


namespace Neuron
{
  class W32EventProcessor
  {
    public:
      // This is a callback for Windows events
      // Returns 0 if the event is handled here, -1 otherwise
      virtual LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;

      // The window has just lost focus, so anything a processor believes is
      // held is no longer held — Windows will not send the release. Default is
      // to do nothing; the input driver overrides it to let go of everything.
      virtual void OnFocusLost() {}
  };
} // namespace Neuron
