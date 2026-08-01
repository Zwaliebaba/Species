#ifndef _included_treewindow_h
#define _included_treewindow_h


#ifdef LOCATION_EDITOR


#include "DarwiniaWindow.h"


class TreeWindow : public DarwiniaWindow
{
public:
    int m_selectionId;

public:
    TreeWindow( char const *_name );

    void Create();
    void Update();
};


#endif // LOCATION_EDITOR

#endif
