
#pragma once

#include "EclWindow.h"
#include "EclButton.h"

// ============================================================================
// High level management


namespace Neuron
{
  void EclInitialise(int screenW, int screenH);

  // THREE ENTRY POINTS WHERE EclUpdateMouse WAS ONE. It took the polled button
  // booleans and reconstructed the edges by comparing them with the pair it had
  // kept from last frame — which is a second re-derivation of information the
  // OS had already given away, and it could not represent a press and a release
  // that happened between two frames at all. The edges are events now.
  //
  // EclMouseDown returns TRUE if the click landed in a window, which is the
  // answer the router turns into "the UI took that click". EclMouseUp needs no
  // answer: Eclipse keys a release off the position the PRESS was made at, and
  // the router keys consumption off what that press did, so both already agree
  // without asking twice.
  bool EclMouseDown(int mouseX, int mouseY, bool rightButton);
  void EclMouseUp(int mouseX, int mouseY, bool rightButton);

  // Where the cursor is, called once per FRAME rather than per event, and
  // deliberately: hover highlighting, the one-second tooltip delay and window
  // dragging are all things that have to keep happening while the mouse is
  // perfectly still, and a still mouse produces no events whatsoever.
  void EclMouseMove(int mouseX, int mouseY);

  void EclUpdateKeyboard(int keyCode, bool shift, bool ctrl, bool alt);

  // One decoded character, offered to whatever the focused window has in text
  // edit. Returns TRUE if a widget took it — the caller uses that to stop the
  // key that produced it from also firing a game binding, which is the capture
  // rule the router formalises in T8. A character Windows decoded is not a
  // virtual key code: it is already correct for the user's keyboard layout,
  // which is the whole reason this exists alongside EclUpdateKeyboard.
  bool EclUpdateChar(unsigned int character);
  void EclRender();
  void EclUpdate();

  void EclShutdown();


  // ============================================================================
  // Window management


  // Both take ownership. The window list holds it from here on.
  void EclRegisterWindow(std::unique_ptr<EclWindow> window, EclWindow* parent = nullptr);
  void EclRemoveWindow(std::string_view name);
  void EclRegisterPopup(std::unique_ptr<EclWindow> window);
  void EclRemovePopup();

  // Every name below is only ever COMPARED against EclWindow::m_name or copied
  // into one of this module's std::string statics — none of them reaches a C
  // API — so they are string_view since strings-modernised T13. That is what
  // retires the .c_str() every caller wrote after T11 made m_name a
  // std::string.
  void EclBringWindowToFront(std::string_view name);
  void EclSetWindowPosition(std::string_view name, int x, int y);
  void EclSetWindowSize(std::string_view name, int w, int h);

  int EclGetWindowIndex(std::string_view name); // -1 = failure
  EclWindow* EclGetWindow(std::string_view name);
  EclWindow* EclGetWindow(int x, int y);

  bool EclMouseInWindow(EclWindow* window);
  bool EclMouseInButton(EclWindow* window, EclButton* button);
  bool EclIsTextEditing();

  void EclRegisterTooltipCallback(void (*_callback)(EclWindow*, EclButton*));

  void EclMaximiseWindow(std::string_view name);
  void EclUnMaximise();

  // The three accessors below still return char const*, deliberately: they hand
  // back a pointer into a static this module reassigns, and narrowing a return
  // to string_view preserves that lifetime bug rather than fixing it. See
  // CODING_STANDARDS.md, Strings.
  char const* EclGetCurrentButton();
  char const* EclGetCurrentClickedButton();

  char const* EclGetCurrentFocus();
  void EclSetCurrentFocus(std::string_view name);

  char const* EclGenerateUniqueWindowName(std::string_view name); // In static mem (don't delete!)
  std::vector<std::unique_ptr<EclWindow>>* EclGetWindows();

  // ============================================================================
  // Other

  int EclGetAccurateTime();
  int EclGetScreenW();
  int EclGetScreenH();
} // namespace Neuron
