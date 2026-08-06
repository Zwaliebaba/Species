#include "pch.h"
#include "Preferences.h"
#include "TextRenderer.h"
#include "Profiler.h"
#include "LanguageTable.h"

#include "PrefsSoundWindow.h"
#include "DropDownMenu.h"

#include "SoundSystem.h"
#include "SampleCache.h"

#include "WorldPointers.h"


#define SOUND_CHANNELS "SoundChannels"
#define SOUND_SWAPSTEREO "SoundSwapStereo"
#define SOUND_DSPEFFECTS "SoundDSP"
#define SOUND_MEMORY "SoundMemoryUsage"


namespace Species
{
  class RestartSoundButton : public SpeciesButton
  {
    public:
      void MouseUp()
      {
        PrefsSoundWindow* parent = (PrefsSoundWindow*)m_parent;

        int oldMemoryUsage = g_prefsManager->GetInt(SOUND_MEMORY);

        g_prefsManager->SetInt(SOUND_CHANNELS, parent->m_numChannels);
        g_prefsManager->SetInt(SOUND_SWAPSTEREO, parent->m_swapStereo);
        g_prefsManager->SetInt(SOUND_DSPEFFECTS, parent->m_dspEffects);
        g_prefsManager->SetInt(SOUND_MEMORY, parent->m_memoryUsage);

        g_soundSystem->RestartSoundLibrary();

        if (parent->m_memoryUsage != oldMemoryUsage)
        {
          //
          // Clear out the sample cache

          g_cachedSampleManager.EmptyCache();

          for (int i = 0; i < g_soundSystem->m_sounds.Size(); ++i)
          {
            if (g_soundSystem->m_sounds.ValidIndex(i))
            {
              SoundInstance* instance = g_soundSystem->m_sounds[i].get();
              if (instance->m_cachedSampleHandle)
              {
                g_deletingCachedSampleHandle = true;
                delete instance->m_cachedSampleHandle;
                instance->m_cachedSampleHandle = nullptr;
                g_deletingCachedSampleHandle = false;
                instance->OpenStream(false);
              }
            }
          }

          if (g_soundSystem->m_music && g_soundSystem->m_music->m_cachedSampleHandle)
          {
            g_deletingCachedSampleHandle = true;
            delete g_soundSystem->m_music->m_cachedSampleHandle;
            g_soundSystem->m_music->m_cachedSampleHandle = nullptr;
            g_deletingCachedSampleHandle = false;
            g_soundSystem->m_music->OpenStream(false);
          }
        }


        if (parent->m_dspEffects)
        {
          g_soundSystem->PropagateBlueprints();
          // Causes all sounds to reload their DSP effects from the blueprints
        }
        else
        {
          g_soundSystem->StopAllDSPEffects();
        }

        g_prefsManager->Save();
      }
  };


  PrefsSoundWindow::PrefsSoundWindow()
    : SpeciesWindow(LANGUAGEPHRASE("dialog_soundoptions"))
  {
    SetMenuSize(532, 390);
    SetPosition(g_renderer->ScreenW() / 2 - m_w / 2, g_renderer->ScreenH() / 2 - m_h / 2);

    m_numChannels = g_prefsManager->GetInt(SOUND_CHANNELS, 16);
    m_swapStereo = g_prefsManager->GetInt(SOUND_SWAPSTEREO, 0);
    m_dspEffects = g_prefsManager->GetInt(SOUND_DSPEFFECTS, 1);
    m_memoryUsage = g_prefsManager->GetInt(SOUND_MEMORY, 1);
  }


  void PrefsSoundWindow::Create()
  {
    SpeciesWindow::Create();

    /*int x = GetMenuSize(150);
    int w = GetMenuSize(170);
    int y = GetMenuSize(30);
    int h = GetMenuSize(30);
  int fontSize = GetMenuSize(13);*/

    int y = GetClientRectY1() + GetMenuSize(30);
    int border = GetClientRectX1() + 10;
    int x = m_w / 2;
    int buttonH = GetMenuSize(20);
    int buttonW = m_w / 2 - border * 2;
    int h = buttonH + border;
    int fontSize = GetMenuSize(13);

    InvertedBox* box = new InvertedBox();
    box->SetShortProperties("invert", 10, y += border, m_w - 20, GetClientRectY2() - h * 3 - y);
    RegisterButton(box);

    DropDownMenu* numChannels = new DropDownMenu();
    numChannels->SetShortProperties(LANGUAGEPHRASE("dialog_numchannels"), x, y += border, buttonW, buttonH);
    numChannels->AddOption(LANGUAGEPHRASE("dialog_8channels"), 8);
    numChannels->AddOption(LANGUAGEPHRASE("dialog_16channels"), 16);
    numChannels->AddOption(LANGUAGEPHRASE("dialog_32channels"), 32);
    numChannels->AddOption(LANGUAGEPHRASE("dialog_64channels"), 64);
    numChannels->RegisterInt(&m_numChannels);
    numChannels->m_fontSize = fontSize;
    RegisterButton(numChannels);
    m_buttonOrder.push_back(numChannels);

    DropDownMenu* memoryUsage = new DropDownMenu();
    memoryUsage->SetShortProperties(LANGUAGEPHRASE("dialog_memoryusage"), x, y += h, buttonW, buttonH);
    memoryUsage->AddOption(LANGUAGEPHRASE("dialog_high"), 1);
    memoryUsage->AddOption(LANGUAGEPHRASE("dialog_medium"), 2);
    memoryUsage->AddOption(LANGUAGEPHRASE("dialog_low"), 3);
    memoryUsage->RegisterInt(&m_memoryUsage);
    memoryUsage->m_fontSize = fontSize;
    RegisterButton(memoryUsage);
    m_buttonOrder.push_back(memoryUsage);

    DropDownMenu* swapStereo = new DropDownMenu();
    swapStereo->SetShortProperties(LANGUAGEPHRASE("dialog_swapstereo"), x, y += h, buttonW, buttonH);
    swapStereo->AddOption(LANGUAGEPHRASE("dialog_enabled"), 1);
    swapStereo->AddOption(LANGUAGEPHRASE("dialog_disabled"), 0);
    swapStereo->RegisterInt(&m_swapStereo);
    swapStereo->m_fontSize = fontSize;
    RegisterButton(swapStereo);
    m_buttonOrder.push_back(swapStereo);

    DropDownMenu* dspEffects = new DropDownMenu();
    dspEffects->SetShortProperties(LANGUAGEPHRASE("dialog_realtimeeffects"), x, y += h, buttonW, buttonH);
    dspEffects->AddOption(LANGUAGEPHRASE("dialog_enabled"), 1);
    dspEffects->AddOption(LANGUAGEPHRASE("dialog_disabled"), 0);
    dspEffects->RegisterInt(&m_dspEffects);
    dspEffects->m_fontSize = fontSize;
    RegisterButton(dspEffects);
    m_buttonOrder.push_back(dspEffects);

    y = m_h - h;

    CloseButton* cancel = new CloseButton();
    cancel->SetShortProperties(LANGUAGEPHRASE("dialog_close"), border, y, buttonW, buttonH);
    cancel->m_fontSize = fontSize;
    cancel->m_centered = true;
    RegisterButton(cancel);
    m_buttonOrder.push_back(cancel);

    RestartSoundButton* apply = new RestartSoundButton();
    apply->SetShortProperties(LANGUAGEPHRASE("dialog_apply"), m_w - buttonW - border, y, buttonW, buttonH);
    apply->m_fontSize = fontSize;
    apply->m_centered = true;
    RegisterButton(apply);
    m_buttonOrder.push_back(apply);
  }


void PrefsSoundWindow::Render(bool _hasFocus)
{
  SpeciesWindow::Render(_hasFocus);

  int border = GetClientRectX1() + 10;
  int x = m_x + 20;
  int y = m_y + GetClientRectY1() + border * 2 + GetMenuSize(30);
  int h = GetMenuSize(20) + border;
  int size = GetMenuSize(13);

  g_editorFont.DrawText2D(x, y += border, size, LANGUAGEPHRASE("dialog_numchannels"));
  g_editorFont.DrawText2D(x, y += h, size, LANGUAGEPHRASE("dialog_memoryusage"));
  g_editorFont.DrawText2D(x, y += h, size, LANGUAGEPHRASE("dialog_swapstereo"));
  g_editorFont.DrawText2D(x, y += h, size, LANGUAGEPHRASE("dialog_realtimeeffects"));


#ifdef PROFILER_ENABLED
  const auto& children = g_profiler->m_rootElement->m_children;
  const auto found = children.find("Advance SoundSystem");
  ProfiledElement* element = (found == children.end()) ? nullptr : found->second.get();
  if (element->m_lastNumCalls > 0)
  {
    float occup = element->m_lastTotalTime * 100;
    if (occup > 15)
      glColor4f(1.0f, 0.3f, 0.3f, 1.0f);
    g_editorFont.DrawText2DCentre(m_x + m_w / 2, m_y + m_h - GetMenuSize(50), GetMenuSize(17), "{} {}%", LANGUAGEPHRASE("dialog_cpuusage"),
                                  int(occup));
  }
  else
  {
    glColor4f(1.0f, 0.3f, 0.3f, 1.0f);
    g_editorFont.DrawText2DCentre(m_x + m_w / 2, m_y + m_h - GetMenuSize(50), size, LANGUAGEPHRASE("dialog_cpuusageunknown"));
  }
#endif

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  float memoryUsage = g_cachedSampleManager.GetMemoryUsage();
  memoryUsage /= 1024.0f;
  memoryUsage /= 1024.0f;
  g_editorFont.DrawText2DCentre(m_x + m_w / 2, m_y + m_h - GetMenuSize(70), size, "{} {:2.1f} Mb", LANGUAGEPHRASE("dialog_memoryusage"), memoryUsage);
}
} // namespace Species
