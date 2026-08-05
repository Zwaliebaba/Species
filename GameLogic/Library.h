
#pragma once

#include "Building.h"

#include "GlobalWorld.h"


namespace Species
{
  class Library : public Building
  {
    public:
      bool m_scrollSpawned[GlobalResearch::NumResearchItems];

    public:
      Library();

      bool Advance();
  };
} // namespace Species
