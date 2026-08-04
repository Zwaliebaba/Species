
#pragma once

#include "EclWindow.h"
#include "EclButton.h"

// ============================================================================
// High level management

void EclInitialise(int screenW, int screenH);

void EclUpdateMouse(int mouseX, int mouseY, bool lmb, bool rmb);
void EclUpdateKeyboard(int keyCode, bool shift, bool ctrl, bool alt);
void EclRender();
void EclUpdate();

void EclShutdown();


// ============================================================================
// Window management


// Both take ownership. The window list holds it from here on.
void EclRegisterWindow(std::unique_ptr<EclWindow> window, EclWindow* parent = nullptr);
void EclRemoveWindow(char const* name);
void EclRegisterPopup(std::unique_ptr<EclWindow> window);
void EclRemovePopup();

void EclBringWindowToFront(char* name);
void EclSetWindowPosition(char const* name, int x, int y);
void EclSetWindowSize(char const* name, int w, int h);

int EclGetWindowIndex(char const* name); // -1 = failure
EclWindow* EclGetWindow(char const* name);
EclWindow* EclGetWindow(int x, int y);

bool EclMouseInWindow(EclWindow* window);
bool EclMouseInButton(EclWindow* window, EclButton* button);
bool EclIsTextEditing();

void EclRegisterTooltipCallback(void (*_callback)(EclWindow*, EclButton*));

void EclMaximiseWindow(char const* name);
void EclUnMaximise();

char const* EclGetCurrentButton();
char const* EclGetCurrentClickedButton();

char const* EclGetCurrentFocus();
void EclSetCurrentFocus(const char* name);

char const* EclGenerateUniqueWindowName(char const* name); // In static mem (don't delete!)
std::vector<std::unique_ptr<EclWindow>>* EclGetWindows();

// ============================================================================
// Other

int EclGetAccurateTime();
int EclGetScreenW();
int EclGetScreenH();
