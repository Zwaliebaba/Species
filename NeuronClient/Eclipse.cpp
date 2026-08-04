#include "pch.h"
#include "Random.h"
#include "Eclipse.h"

// ============================================================================

static std::vector<std::unique_ptr<EclWindow>> windows;

static void (*tooltipCallback)(EclWindow*, EclButton*) = nullptr;

static int buttonDownMouseX = 0;
static int buttonDownMouseY = 0;

static int mouseX = 0;
static int mouseY = 0;
static bool lmb = false;
static bool rmb = false;

static int screenW = 0;
static int screenH = 0;

static int mouseDownWindowX = 0;
static int mouseDownWindowY = 0;

static int tooltipTimer = 0;

static int maximiseOldX = 0;
static int maximiseOldY = 0;
static int maximiseOldW = 0;
static int maximiseOldH = 0;

// "None" is a sentinel NAME, not an empty string: callers compare against the
// literal, and EclGetWindow("None") is expected to miss. Kept as it was.
static std::string mouseDownWindow = "None"; // Which window have I just clicked in
static std::string windowFocus = "None";     // Which window is at the front
static std::string popupWindow = "None";     // Current popup window
static std::string maximisedWindow = "None"; // Which window is maximised
static std::string currentButton = "None";   // Current highlighted button

void EclInitialise(int _screenW, int _screenH)
{
  screenW = _screenW;
  screenH = _screenH;
}

void EclUpdateMouse(int _mouseX, int _mouseY, bool _lmb, bool _rmb)
{
  int oldMouseX = mouseX;
  int oldMouseY = mouseY;

  mouseX = _mouseX;
  mouseY = _mouseY;

  if (!_lmb && !lmb && !_rmb && !rmb) // No buttons changed, mouse move only
  {
    EclWindow* currentWindow = EclGetWindow(windowFocus.c_str());
    if (currentWindow)
    {
      EclButton* button = currentWindow->GetButton(mouseX - currentWindow->m_x, mouseY - currentWindow->m_y);
      if (button)
      {
        if (currentButton != button->m_name)
        {
          currentButton = button->m_name;
          tooltipTimer = EclGetAccurateTime() + 1000;
        }
        else
        {
          if (EclGetAccurateTime() > tooltipTimer)
          {
            if (tooltipCallback)
              tooltipCallback(currentWindow, button);
          }
        }
      }
      else
      {
        if (currentButton != "None")
        {
          currentButton = "None";
          if (tooltipCallback)
            tooltipCallback(nullptr, nullptr);
        }
      }
    }
    else
    {
      if (currentButton != "None")
      {
        currentButton = "None";
        if (tooltipCallback)
          tooltipCallback(nullptr, nullptr);
      }
    }
  }
  else if (_lmb && !lmb) // Left button down
  {
    buttonDownMouseX = mouseX;
    buttonDownMouseY = mouseY;

    EclWindow* currentWindow = EclGetWindow(mouseX, mouseY);
    if (currentWindow)
    {
      windowFocus = currentWindow->m_name;
      EclBringWindowToFront(currentWindow->m_name);
      mouseDownWindow = currentWindow->m_name;

      EclButton* button = currentWindow->GetButton(mouseX - currentWindow->m_x, mouseY - currentWindow->m_y);
      if (button)
      {
        currentButton = button->m_name;
        button->MouseDown();
      }
      else
      {
        currentButton = "None";
        currentWindow->MouseEvent(true, false, false, true);
        if (currentWindow->m_movable)
        {
          mouseDownWindowX = mouseX - currentWindow->m_x;
          mouseDownWindowY = mouseY - currentWindow->m_y;
        }
      }
    }
    else
    {
      if (windowFocus != "None")
      {
        windowFocus = "None";
      }
    }

    lmb = true;
  }
  else if (_lmb && lmb) // Left button dragged
  {
    buttonDownMouseX = mouseX;
    buttonDownMouseY = mouseY;

    if (mouseDownWindow != "None")
    {
      EclWindow* window = EclGetWindow(mouseDownWindow.c_str());
      EclButton* button = window->GetButton(mouseX - window->m_x, mouseY - window->m_y);

      if (button)
      {
        button->MouseDown();
      }
      else
      {
        if (currentButton == "None")
        {
          int newWidth = window->m_w;
          int newHeight = window->m_h;
          bool sizeChanged = false;

          if (oldMouseY > window->m_y + window->m_h - 4 && oldMouseY < window->m_y + window->m_h + 4)
          {
            newHeight = mouseY - window->m_y;
            sizeChanged = true;
          }

          if (oldMouseX > window->m_x + window->m_w - 4 && oldMouseX < window->m_x + window->m_w + 4)
          {
            newWidth = mouseX - window->m_x;
            sizeChanged = true;
          }

          if (sizeChanged)
          {
            if (window->m_resizable)
            {
              if (newWidth < 60)
                newWidth = 60;
              if (newHeight < 40)
                newHeight = 40;
              EclSetWindowSize(mouseDownWindow.c_str(), newWidth, newHeight);
            }
          }
          else
          {
            if (window->m_movable)
            {
              EclSetWindowPosition(mouseDownWindow.c_str(), mouseX - mouseDownWindowX, mouseY - mouseDownWindowY);
            }
          }
        }
      }
    }
  }
  else if (!_lmb && lmb) // Left button up
  {
    mouseDownWindow = "None";
    lmb = false;

    EclWindow* currentWindow = EclGetWindow(buttonDownMouseX, buttonDownMouseY);
    if (currentWindow)
    {
      EclButton* button = currentWindow->GetButton(buttonDownMouseX - currentWindow->m_x, buttonDownMouseY - currentWindow->m_y);
      if (button)
      {
        button->MouseUp();
      }
      else
      {
        currentWindow->MouseEvent(true, false, true, false);
        EclRemovePopup();
      }
    }
    else
    {
      EclRemovePopup();
    }
  }

  if (_rmb && !rmb)
  {
    // Right button down
  }
  else if (_rmb && rmb)
  {
    // Right button dragged
  }
  else if (!_rmb && rmb)
  {
    // Right button up
  }
}

void EclUpdateKeyboard(int keyCode, bool shift, bool ctrl, bool alt)
{
  EclWindow* currentWindow = EclGetWindow(windowFocus.c_str());
  if (currentWindow)
  {
    currentWindow->Keypress(keyCode, shift, ctrl, alt);
  }
}

void EclRegisterTooltipCallback(void (*_callback)(EclWindow*, EclButton*)) { tooltipCallback = _callback; }

void EclRender()
{
  bool maximiseRender = false;

  //
  // Render any maximised Window?

  if (maximisedWindow != "None")
  {
    EclWindow* maximised = EclGetWindow(maximisedWindow.c_str());
    if (maximised)
    {
      maximised->Render(true);
      maximiseRender = true;
    }
    else
    {
      EclUnMaximise();
    }
  }

  if (!maximiseRender)
  {
    //
    // Draw all dirty buttons

    for (int i = windows.size() - 1; i >= 0; --i)
    {
      EclWindow* window = windows[i].get();
      bool hasFocus = (windowFocus == window->m_name);
      window->Render(hasFocus);
    }
  }
}

void EclUpdate()
{
  //
  // Update all windows

  for (int i = 0; i < windows.size(); ++i)
  {
    EclWindow* window = windows[i].get();
    window->Update();
  }
}

void EclShutdown()
{
  windows.clear();
}

char const* EclGetCurrentButton() { return currentButton.c_str(); }

char const* EclGetCurrentClickedButton()
{
  if (lmb)
    return currentButton.c_str();

  else
    return "None";
}

char const* EclGenerateUniqueWindowName(char const* name)
{
  // Static because the return type is a pointer and callers hold it long
  // enough to register a window with it.
  static std::string uniqueName;

  int index = 1;
  uniqueName = name;
  while (EclGetWindow(uniqueName.c_str()))
  {
    ++index;
    uniqueName = std::format("{}{}", name, index);
  }

  return uniqueName.c_str();
}

void EclRegisterWindow(std::unique_ptr<EclWindow> owned, EclWindow* parent)
{
  // The observer is taken before the owner moves into the list, because the
  // rest of this function — and EclDirtyWindow after it — works through it.
  EclWindow* window = owned.get();

  //    DebugAssert( window );

  if (EclGetWindow(window->m_name))
  {
  }

  if (parent && window->m_x == 0 && window->m_y == 0)
  {
    // We should place the window in a decent location
    int left = screenW / 2 - parent->m_x;
    int above = screenH / 2 - parent->m_y;
    if (left > window->m_w / 2)
      window->m_x = int(parent->m_x + parent->m_w * (float)speciesRandom() / (float)SPECIES_RAND_MAX);
    else
      window->m_x = int(parent->m_x - window->m_w * (float)speciesRandom() / (float)SPECIES_RAND_MAX);
    if (above > window->m_h / 2)
      window->m_y = int(parent->m_y + parent->m_h * (float)speciesRandom() / (float)SPECIES_RAND_MAX);
    else
      window->m_y = int(parent->m_y - window->m_h / 2 * (float)speciesRandom() / (float)SPECIES_RAND_MAX);
  }

  window->MakeAllOnScreen();
  windows.insert(windows.begin(), std::move(owned));
  window->Create();
}

void EclRegisterPopup(std::unique_ptr<EclWindow> window)
{
  EclRemovePopup();
  // DebugAssert( window );
  popupWindow = window->m_name;
  EclRegisterWindow(std::move(window));
}

void EclRemovePopup()
{
  if (EclGetWindow(popupWindow.c_str()))
  {
    EclRemoveWindow(popupWindow.c_str());
  }
  popupWindow = "None";
}

void EclRemoveWindow(char const* name)
{
  int index = EclGetWindowIndex(name);
  if (index != -1)
  {
    std::unique_ptr<EclWindow> owned = std::move(windows[index]);
    windows.erase(windows.begin() + (index));
    // Destroyed where the delete was, after the erase.
    owned.reset();

    if (mouseDownWindow == name)
    {
      mouseDownWindow = "None";
    }

    if (windowFocus == name)
    {
      windowFocus = "None";
    }
  }
  else
  {
  }
}

void EclSetWindowPosition(char const* name, int x, int y)
{
  EclWindow* window = EclGetWindow(name);
  if (window)
  {
    window->m_x = x;
    window->m_y = y;
  }
}

void EclSetWindowSize(char const* name, int w, int h)
{
  EclWindow* window = EclGetWindow(name);
  if (window)
  {
    window->Remove();
    window->m_w = w;
    window->m_h = h;
    window->Create();
  }
  else
  {
  }
}

void EclBringWindowToFront(char* name)
{
  int index = EclGetWindowIndex(name);
  if (index != -1)
  {
    std::unique_ptr<EclWindow> owned = std::move(windows[index]);
    EclWindow* window = owned.get();
    windows.erase(windows.begin() + (index));
    windows.insert(windows.begin(), std::move(owned));
  }
  else
  {
  }
}

bool EclMouseInWindow(EclWindow* window)
{
  // DebugAssert( window );
  return (EclGetWindow(mouseX, mouseY) == window);
}

bool EclMouseInButton(EclWindow* window, EclButton* button)
{
  return (EclMouseInWindow(window) && mouseX >= window->m_x + button->m_x && mouseX <= window->m_x + button->m_x + button->m_w &&
          mouseY >= window->m_y + button->m_y && mouseY <= window->m_y + button->m_y + button->m_h);
}

bool EclIsTextEditing()
{
  EclWindow* currentWindow = EclGetWindow(windowFocus.c_str());
  return (currentWindow && strcmp(currentWindow->m_currentTextEdit, "None") != 0);
}

int EclGetWindowIndex(char const* name)
{
  for (int i = 0; i < windows.size(); ++i)
  {
    EclWindow* window = windows[i].get();
    if (strcmp(window->m_name, name) == 0)
      return i;
  }

  return -1;
}

EclWindow* EclGetWindow(char const* name)
{
  int index = EclGetWindowIndex(name);
  if (index == -1)
  {
    return nullptr;
  }
  else
  {
    return windows[index].get();
  }
}

EclWindow* EclGetWindow(int x, int y)
{
  for (int i = 0; i < windows.size(); ++i)
  {
    EclWindow* window = windows[i].get();
    if (x >= window->m_x && x <= window->m_x + window->m_w && y >= window->m_y && y <= window->m_y + window->m_h)
    {
      return window;
    }
  }

  return nullptr;
}

void EclMaximiseWindow(char const* name)
{
  EclUnMaximise();
  EclWindow* w = EclGetWindow(name);
  if (w)
  {
    maximisedWindow = name;
    mouseDownWindow = name;
    windowFocus = name;
    maximiseOldX = w->m_x;
    maximiseOldY = w->m_y;
    maximiseOldW = w->m_w;
    maximiseOldH = w->m_h;
    w->SetPosition(0, 0);
    w->SetSize(screenW, screenH);
  }
}

void EclUnMaximise()
{
  EclWindow* w = EclGetWindow(maximisedWindow.c_str());
  maximisedWindow = "None";

  if (w)
  {
    w->SetPosition(maximiseOldX, maximiseOldY);
    w->SetSize(maximiseOldW, maximiseOldH);
  }
}

std::vector<std::unique_ptr<EclWindow>>* EclGetWindows() { return &windows; }

int EclGetAccurateTime() { return GetTickCount(); }

int EclGetScreenW() { return screenW; }

int EclGetScreenH() { return screenH; }

char const* EclGetCurrentFocus() { return windowFocus.c_str(); }

void EclSetCurrentFocus(const char* name)
{
  if (strlen(name) < SIZE_ECLWINDOW_NAME)
  {
    windowFocus = name;
  }
}
