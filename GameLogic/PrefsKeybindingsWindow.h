#pragma once


#include <memory>
#include <vector>

#include "Input.h"
#include "SpeciesWindow.h"


// Owning — see NeuronClient/InputSpecList.h.
typedef std::vector<std::unique_ptr<InputDescription>> InputDescList;


class PrefsKeybindingsWindow : public SpeciesWindow
{
  public:
    InputDescList m_bindings;
    int m_numMouseButtons;
    int m_controlMethod;

  public:
    PrefsKeybindingsWindow();

    void Create();
    void Remove();

    void Render(bool _hasFocus);
};
