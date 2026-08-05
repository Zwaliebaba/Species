
#pragma once

#include "SpeciesWindow.h"


namespace Species
{
  class PrefsOtherWindow : public SpeciesWindow
  {
    public:
      int m_helpEnabled;
      int m_controlHelpEnabled;
      int m_bootLoader;
      int m_christmas;
      int m_language;
      int m_difficulty;
      int m_largeMenus;
      int m_automaticCamera;

      std::vector<char*> m_languages;

    public:
      PrefsOtherWindow();

      void Create();
      void Render(bool _hasFocus);

      void ListAvailableLanguages();
  };

  // The OTHER_* preference keys moved to NeuronCore/Preferences.h. They are key
  // names rather than anything to do with this window, and leaving them here meant
  // Preferences.cpp had to include a UI header to name its own defaults.
} // namespace Species
