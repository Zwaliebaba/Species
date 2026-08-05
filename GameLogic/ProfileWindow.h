#pragma once

#ifdef PROFILER_ENABLED


#include "SpeciesWindow.h"


class ProfiledElement;


//*****************************************************************************
// Class ProfileWindow
//*****************************************************************************


namespace Species
{
  class ProfileWindow : public SpeciesWindow
  {
    protected:
      int m_yPos;

      void RenderElementProfile(ProfiledElement* _pe, unsigned int _indent);


    public:
      bool m_totalPerSecond;

      ProfileWindow(char const* name);
      ~ProfileWindow();

      void Render(bool hasFocus);
      void Create();
      void Remove();
  };


#endif // PROFILER_ENABLED
} // namespace Species
