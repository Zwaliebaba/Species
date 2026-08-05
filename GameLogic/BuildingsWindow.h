#pragma once

#ifdef LOCATION_EDITOR

#include "SpeciesWindow.h"


// ****************************************************************************
// Class BuildingEditWindow
// ****************************************************************************

// This window is displayed when a building is selected for editing


namespace Species
{
  class BuildingEditWindow : public SpeciesWindow
  {
    public:
      BuildingEditWindow(char const* name);
      ~BuildingEditWindow();

      void Create();
      void Render(bool hasFocus);
  };


  // ****************************************************************************
  // Class BuildingsCreateWindow
  // ****************************************************************************

  // Top level window that contains a "create" button for every type of builing
  class BuildingsCreateWindow : public SpeciesWindow
  {
    public:
      int m_buildingType;

    public:
      BuildingsCreateWindow(char const* _name);
      ~BuildingsCreateWindow();

      void Create();
  };


#endif // LOCATION_EDITOR
} // namespace Species
