
/*
        A Window object.
        Essentially a container for Eclipse buttons.

        All co-ordinates are RELATIVE to the window

  */


#pragma once

#include <vector>

#define SIZE_ECLWINDOW_NAME 256
#define SIZE_ECLWINDOW_TITLE 256

#include "LList.h"

class EclButton;


class EclWindow
{
  public:
    int m_x;
    int m_y;
    int m_w;
    int m_h;

    char m_name[SIZE_ECLWINDOW_NAME];
    char m_title[SIZE_ECLWINDOW_TITLE];

    bool m_movable;
    bool m_resizable;
    bool m_dirty;

    std::vector<EclButton*> m_buttons;

  public:
    char m_currentTextEdit[SIZE_ECLWINDOW_NAME];

  public:
    EclWindow(char const* _name);
    virtual ~EclWindow();

    void SetName(char const* _name);
    void SetTitle(char const* _title);
    void SetPosition(int _x, int _y);
    void SetSize(int _w, int _h);
    void SetMovable(bool _movable);
    void MakeAllOnScreen();

    void RegisterButton(EclButton* button);
    void RemoveButton(char const* _name);

    void BeginTextEdit(char* _name);
    void EndTextEdit();

    virtual EclButton* GetButton(char const* _name);
    virtual EclButton* GetButton(int _x, int _y);

    virtual void Create();
    virtual void Remove();
    virtual void Update();
    virtual void Render(bool hasFocus);

    virtual void Keypress(int keyCode, bool shift, bool ctrl, bool alt);
    virtual void MouseEvent(bool lmb, bool rmb, bool up, bool down);
};
