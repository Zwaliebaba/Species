
/*
 * ===============
 * SPECIES WINDOW
 * ===============
 *
 * A basic window for use in Species
 *
 */

#pragma once

#include "Eclipse.h"

class InputField;

class SpeciesButton;


class SpeciesWindow : public EclWindow
{
  public:
    std::vector<EclButton*> m_buttonOrder;
    int m_currentButton;
    bool m_buttonChangedThisUpdate;
    bool m_skipUpdate;

  public:
    SpeciesWindow(char const* name);
    ~SpeciesWindow();

    void Create();
    void Remove();
    void Render(bool hasFocus);
    void Update();

    // One overload per registerable type, rather than one function taking a type
    // tag and a void*. The void* accepted any pointer at all — passing a
    // char[256] where a float* was meant compiled and corrupted memory at run
    // time — and four call sites were already passing a char(*)[256] where a
    // char* was intended. See tasks/strings-modernised.yaml T14.
    void CreateValueControl(char const* name, unsigned char* value, int y, float change, float _lowBound, float _highBound,
                            SpeciesButton* callback = nullptr, int x = -1, int w = -1);
    // bool exists as its own overload because four call sites were registering
    // a bool member through the void* as if it were an unsigned char, and
    // removing the void* has to keep doing exactly that rather than silently
    // change what the editor writes. See T14's notes.
    void CreateValueControl(char const* name, bool* value, int y, float change, float _lowBound, float _highBound, SpeciesButton* callback = nullptr,
                            int x = -1, int w = -1);
    void CreateValueControl(char const* name, int* value, int y, float change, float _lowBound, float _highBound, SpeciesButton* callback = nullptr,
                            int x = -1, int w = -1);
    void CreateValueControl(char const* name, float* value, int y, float change, float _lowBound, float _highBound, SpeciesButton* callback = nullptr,
                            int x = -1, int w = -1);
    // Text fields have no scroller buttons and take the full width, which is the
    // only behavioural difference the old dataType switch encoded. char* rather
    // than std::string* until strings-modernised T5 converts InputField.
    void CreateValueControl(char const* name, char* value, int y, float change, float _lowBound, float _highBound, SpeciesButton* callback = nullptr,
                            int x = -1, int w = -1);

    void CreateColourControl(char const* name, int* value, int y, SpeciesButton* callback = nullptr, int x = -1, int w = -1);

    void RemoveValueControl(char* name);

  private:
    // The shared body of the four overloads above, split so that neither of
    // them needs a type tag. CreateInputField does everything up to the
    // Register call; CreateValueScrollers adds the < and > buttons that every
    // non-text field carries.
    InputField* CreateInputField(char const* name, int y, float _lowBound, float _highBound, SpeciesButton* callback, int x, int w, bool isTextField);
    void CreateValueScrollers(char const* name, InputField* input, int y, float change);

  public:
    int GetMenuSize(int _value);
    void SetMenuSize(int _w, int _h);

    int GetClientRectX1();
    int GetClientRectY1();
    int GetClientRectX2();
    int GetClientRectY2();

    void SetCurrentButton(EclButton* button);
};

class SpeciesButton : public EclButton
{
  public:
    float m_fontSize;
    bool m_centered;
    bool m_disabled;
    bool m_highlightedThisFrame;
    bool m_mouseHighlightMode;

  public:
    SpeciesButton();

    void Render(int realX, int realY, bool highlighted, bool clicked);
    virtual void SetShortProperties(char const* _name, int x, int y, int w = -1, int h = -1, char* _caption = nullptr, char* _tooltip = nullptr);
    void SetDisabled(bool _disabled = true);
    void UpdateButtonHighlight();
};


class BorderlessButton : public SpeciesButton
{
  public:
    BorderlessButton();
    void Render(int realX, int realY, bool highlighted, bool clicked);
    void SetShortProperties(char const* _name, int x, int y, int w = -1, int h = -1, char* _caption = nullptr, char* _tooltip = nullptr);
};


class CloseButton : public SpeciesButton
{
  public:
    bool m_iconised;

  public:
    CloseButton();
    void Render(int realX, int realY, bool highlighted, bool clicked);
    void MouseUp();
};


class GameExitButton : public SpeciesButton
{
    void MouseUp();
};


class InvertedBox : public SpeciesButton
{
  public:
    void Render(int realX, int realY, bool highlighted, bool clicked);
};


class LabelButton : public SpeciesButton
{
  public:
    void Render(int realX, int realY, bool highlighted, bool clicked);
};
