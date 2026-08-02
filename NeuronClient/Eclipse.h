
/*
 *          ECLIPSE
 *        version 2.0
 *
 *
 *  Generic Interface Library
 *
 */

#ifndef _included_eclipse_h
#define _included_eclipse_h

#include "EclWindow.h"
#include "EclButton.h"

// ============================================================================
// High level management

void EclInitialise ( int screenW, int screenH );

void EclUpdateMouse     ( int mouseX, int mouseY, bool lmb, bool rmb );
void EclUpdateKeyboard  ( int keyCode, bool shift, bool ctrl, bool alt );
void EclRender ();
void EclUpdate ();

void EclShutdown ();


// ============================================================================
// Window management


void EclRegisterWindow          ( EclWindow *window, EclWindow *parent=nullptr );
void EclRemoveWindow            ( char const *name );
void EclRegisterPopup           ( EclWindow *window );
void EclRemovePopup             ();

void EclBringWindowToFront      ( char *name );
void EclSetWindowPosition       ( char *name, int x, int y );
void EclSetWindowSize           ( char *name, int w, int h );

int EclGetWindowIndex           ( char const *name );                                             // -1 = failure
EclWindow *EclGetWindow         ( char const *name );
EclWindow *EclGetWindow         ( int x, int y );                                           

bool EclMouseInWindow           ( EclWindow *window );
bool EclMouseInButton           ( EclWindow *window, EclButton *button );
bool EclIsTextEditing			();

void EclRegisterTooltipCallback ( void (*_callback) (EclWindow *, EclButton *) );

void EclMaximiseWindow( char const *name );
void EclUnMaximise              ();

char *EclGetCurrentButton          ();
char const *EclGetCurrentClickedButton   ();

char *EclGetCurrentFocus             ();
void EclSetCurrentFocus              ( char *name );

char *EclGenerateUniqueWindowName( char *name );                                // In static mem (don't delete!)
LList <EclWindow *> *EclGetWindows ();

// ============================================================================
// Dirty rectangles

class DirtyRect
{
public:
    DirtyRect();
    DirtyRect( int newx, int newy, 
               int newwidth, int newheight ); 
	int m_x;
	int m_y;
	int m_width;
	int m_height;
};


void EclRegisterClearFunction   ( void (*_clearDraw) (int, int, int, int) );

void EclDirtyWindow( char const *name );
void EclDirtyWindow             ( EclWindow *window );
void EclDirtyRectangle          ( int x, int y, int w, int h );

bool EclRectangleOverlap        ( int x1, int y1, int w1, int h1,		
                                  int x2, int y2, int w2, int h2 );

void EclResetDirtyRectangles    ();

LList<DirtyRect *> *EclGetDirtyRects ();


// ============================================================================
// Other

int EclGetAccurateTime          ();
int EclGetScreenW				();
int EclGetScreenH				();


#endif
