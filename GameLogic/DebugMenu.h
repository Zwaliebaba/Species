#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class DebugMenu : public SpeciesWindow
  {
    public:
      DebugMenu(char* name);

      void Advance();
      void Create();
      void Render(bool hasFocus);
  };


  class DebugKeyBindings
  {
    public:
      static void DebugMenu();
      static void NetworkButton();
      static void DebugCameraButton();
      static void FollowCameraButton();
      static void FPSButton();
      static void InputLogButton();
#ifdef PROFILER_ENABLED
	static void ProfileButton();
#endif
#ifdef LOCATION_EDITOR
	static void EditorButton();
#endif
#ifdef CHEATMENU_ENABLED
    static void CheatButton();
#endif
	static void ReallyQuitButton();
	static void ToggleFullscreenButton();
  };
} // namespace Species
