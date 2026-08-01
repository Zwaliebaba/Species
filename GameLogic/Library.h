
#ifndef _included_library_h
#define _included_library_h

#include "Building.h"

#include "GlobalWorld.h"



class Library : public Building
{
public:
    bool m_scrollSpawned[GlobalResearch::NumResearchItems];

public:
    Library();

    bool Advance();
};


#endif
