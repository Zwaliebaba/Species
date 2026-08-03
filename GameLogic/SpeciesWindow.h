
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

    void CreateValueControl(char const* name, int dataType, void* value, int y, float change, float _lowBound, float _highBound,
                            SpeciesButton* callback = nullptr, int x = -1, int w = -1);

    void CreateColourControl(char const* name, int* value, int y, SpeciesButton* callback = nullptr, int x = -1, int w = -1);

    void RemoveValueControl(char* name);

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
