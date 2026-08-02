#ifndef _INCLUDED_LANDSCAPE_WINDOW_H
#define _INCLUDED_LANDSCAPE_WINDOW_H

#ifdef LOCATION_EDITOR

#include "SpeciesWindow.h"


class LandscapeFlattenArea;
class LandscapeTile;


// ****************************************************************************
// Class LandscapeTileEditWindow
// ****************************************************************************

class LandscapeTileEditWindow: public SpeciesWindow
{
public:
	LandscapeTile        	*m_tileDef;
    int						m_tileId;

    LandscapeTileEditWindow( char *name, int tileId );
    ~LandscapeTileEditWindow();

	void					Create();
};


// ****************************************************************************
// Class LandscapeFlatAreaEditWindow
// ****************************************************************************

class LandscapeFlattenAreaEditWindow: public SpeciesWindow
{
public:
	LandscapeFlattenArea	*m_areaDef;
    int						m_areaId;

    LandscapeFlattenAreaEditWindow(char const *_name, int areaId);
    ~LandscapeFlattenAreaEditWindow();

	void					Create();
};


// ****************************************************************************
// Class LandscapeEditWindow
// ****************************************************************************

class LandscapeEditWindow: public SpeciesWindow
{
public:
    LandscapeEditWindow( char const *name );
    ~LandscapeEditWindow();

	void					Create();
};


// ****************************************************************************
// Class LandscapeGuideGridWindow
// ****************************************************************************

class LandscapeGuideGridWindow : public SpeciesWindow
{
public:
	LandscapeTile       	*m_tileDef;
    int						m_tileId;
    int                     m_guideGridPower;
    int                     m_pixelSizePerSample;

    enum
    {
        GuideGridToolFreehand,
        GuideGridToolFlatten,
        GuideGridToolBinary
    };
    int                     m_tool;
    float                   m_toolSize;

    LandscapeGuideGridWindow( char *name, int tileId );
    ~LandscapeGuideGridWindow();

    void                    Create();
};


#endif // LOCATION_EDITOR


#endif
