#include "pch.h"

#include <stdio.h>

#include "LanguageTable.h"

#include "InputField.h"
#include "LightsWindow.h"

#include "Location.h"
#include "WorldPointers.h"


#ifdef LOCATION_EDITOR


// ****************************************************************************
// Class LightButton
// ****************************************************************************

class LightButton : public SpeciesButton
{
  public:
    int m_lightNum;

    LightButton(int num)
      : m_lightNum(num)
    {
    }

    void MouseUp()
    {
      if (m_lightNum == -1)
      {
        g_locationEditor->SetTool(LocationEditorAccess::ToolNone);
      }
      else
      {
        g_locationEditor->SetTool(LocationEditorAccess::ToolRotate);
      }
      g_locationEditor->SetSelectionId(m_lightNum);
    }

    void Render(int realX, int realY, bool highlighted, bool clicked)
    {
      if (g_locationEditor->GetSelectionId() == m_lightNum)
      {
        SpeciesButton::Render(realX, realY, highlighted, true);
      }
      else
      {
        SpeciesButton::Render(realX, realY, highlighted, clicked);
      }
    }
};


class LightGammaButton : public SpeciesButton
{
  public:
    int m_lightNum;
    float m_change;

    LightGammaButton(int num)
      : m_lightNum(num)
    {
    }

    void MouseUp()
    {
      Light* light = g_location->m_lights[m_lightNum];
      light->m_colour[0] *= m_change;
      light->m_colour[1] *= m_change;
      light->m_colour[2] *= m_change;
    }
};


class NewLightButton : public SpeciesButton
{
    void MouseUp()
    {
      Light* light = new Light();
      g_location->m_lights.PutData(light);

      EclWindow* parent = m_parent;
      parent->Remove();
      parent->Create();
    }
};


// ****************************************************************************
// Class LightsEditWindow
// ****************************************************************************

LightsEditWindow::LightsEditWindow(char const* name)
  : SpeciesWindow(name)
{
}


LightsEditWindow::~LightsEditWindow() { g_locationEditor->RequestMode(LocationEditorAccess::ModeNone); }


void LightsEditWindow::Create()
{
  SpeciesWindow::Create();

  int height = 5;
  int pitch = 17;
  int buttonWidth = m_w - 50;

  NewLightButton* newLight = new NewLightButton();
  newLight->SetShortProperties(LANGUAGEPHRASE("editor_newlight"), 10, height += pitch, m_w - 20);
  RegisterButton(newLight);

  LightButton* button = new LightButton(-1);
  std::string buttonName;
  buttonName = LANGUAGEPHRASE("editor_deselectlights");
  button->SetShortProperties(buttonName.c_str(), 10, height += pitch, m_w - 20);
  RegisterButton(button);

  height += 6;

  for (int i = 0; i < g_location->m_lights.Size(); i++)
  {
    button = new LightButton(i);

    Light* light = g_location->m_lights.GetData(i);
    buttonName = std::format("{} {}", LANGUAGEPHRASE("editor_selectlight"), i);
    button->SetShortProperties(buttonName.c_str(), 10, height += pitch, m_w - 20);
    RegisterButton(button);

    height += pitch;

    LabelButton* label = new LabelButton();
    label->SetShortProperties(LANGUAGEPHRASE("editor_adjustbrightness"), 10, height, m_w - 50);
    RegisterButton(label);

    LightGammaButton* gammaDown = new LightGammaButton(i);
    buttonName = std::format("down {}", i);
    gammaDown->SetShortProperties(buttonName.c_str(), m_w - 42, height, 15);
    gammaDown->SetCaption("<");
    gammaDown->m_change = 0.9f;
    RegisterButton(gammaDown);

    LightGammaButton* gammaUp = new LightGammaButton(i);
    buttonName = std::format("up {}", i);
    gammaUp->SetShortProperties(buttonName.c_str(), m_w - 25, height, 15);
    gammaUp->SetCaption(">");
    gammaUp->m_change = 1.1f;
    RegisterButton(gammaUp);

    buttonName = std::format("Y{}", i);
    CreateValueControl(buttonName.c_str(), &(light->m_front[1]), height += pitch, 0.01f, -20, 20, nullptr);

    buttonName = std::format("R{}", i);
    CreateValueControl(buttonName.c_str(), &(light->m_colour[0]), height += pitch, 0.02f, 0, 5, nullptr);
    buttonName = std::format("G{}", i);
    CreateValueControl(buttonName.c_str(), &(light->m_colour[1]), height += pitch, 0.02f, 0, 5, nullptr);
    buttonName = std::format("B{}", i);
    CreateValueControl(buttonName.c_str(), &(light->m_colour[2]), height += pitch, 0.02f, 0, 5, nullptr);

    height += 6;
  }
}


#endif // LOCATION_EDITOR
