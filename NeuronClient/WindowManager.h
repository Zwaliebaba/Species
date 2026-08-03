#pragma once

#include <vector>


#include "LList.h"


class WindowManagerWin32;


// ****************************************************************************
// Class Resolution
// ****************************************************************************

class Resolution
{
  public:
    int m_width;
    int m_height;
    std::vector<int> m_refreshRates;

    Resolution(int _width, int _height)
      : m_width(_width),
        m_height(_height)
    {
    }
};


// ****************************************************************************
// Class WindowManager
// ****************************************************************************

class WindowManager
{
  public:
    std::vector<Resolution*> m_resolutions;
    WindowManagerWin32* m_win32Specific;
    bool m_mousePointerVisible;
    bool m_invertY; // Whether the Y coordinate needs to be inverted or not.

  protected:
    int m_screenW;   // Cached values. Use Renderer::ScreenW() if you
    int m_screenH;   // want a value to inspect.
    bool m_windowed; //
    bool m_mouseCaptured;
    bool m_waitVRT;

    int m_mouseOffsetX;
    int m_mouseOffsetY;

    int m_borderWidth;
    int m_titleHeight;

    int m_desktopScreenW;     // Original starting values
    int m_desktopScreenH;     // Original starting values
    int m_desktopColourDepth; // Original starting values
    int m_desktopRefresh;     // Original starting values

    void ListAllDisplayModes();
    bool EnableOpenGL(int _colourDepth, int _zDepth);
    void DisableOpenGL();

  public:
    WindowManager();
    ~WindowManager();

    int GetResolutionId(int _width, int _height); // Returns -1 if resolution doesn't exist
    Resolution* GetResolution(int _id);

    bool CreateWin(int _width, int _height,          // Set _colourDepth, _refreshRate and/or
                   bool _windowed, int _colourDepth, // _zDepth to -1 to get default values
                   int _refreshRate, int _zDepth, bool _waitVRT);

    void DestroyWin();
    void Flip();
    void NastyPollForMessages();
    void NastySetMousePos(int x, int y);
    void NastyMoveMouse(int x, int y);

    void EnsureMouseCaptured();
    void EnsureMouseUncaptured();

    void CaptureMouse();
    void UncaptureMouse();

    void HideMousePointer();
    void UnhideMousePointer();

    bool Windowed();
    bool Captured();
    bool MouseVisible();

    void SaveDesktop();
    void RestoreDesktop();

    void WindowMoved();

    void SuggestDefaultRes(int* _width, int* _height, int* _refresh, int* _depth);

    static void OpenWebsite(char const* _url);
};


// Records the HINSTANCE the process was started with, for the window class
// registration and CreateWindowEx below.
//
// This exists because WinMain does NOT. It used to live in WindowManager.cpp,
// which put an executable's entry point inside a static library and — worse —
// made NeuronClient call AppMain(), a symbol Species defines. That is an upward
// dependency at LINK time, which check_layering cannot see because it reads
// includes. It stayed invisible until GameLogicTests grew tests whose object
// graph reached WindowManager.obj, and the test DLL failed to link against an
// AppMain that only the game executable has. See tasks/layering-inversion.yaml
// T18.
void SetWin32InstanceHandle(HINSTANCE _hInstance);

extern WindowManager* g_windowManager;
