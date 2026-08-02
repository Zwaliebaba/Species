#ifndef INCLUDED_KEYBINDINGS_WINDOW_H
#define INCLUDED_KEYBINDINGS_WINDOW_H


#include "AutoVector.h"
#include "Input.h"
#include "SpeciesWindow.h"


typedef auto_vector<InputDescription> InputDescList;


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

    void Render( bool _hasFocus );
};


#endif
