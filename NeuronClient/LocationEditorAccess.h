#pragma once

// What the layers below Species tell the level editor.
//
// LocationEditor lives in Species. Eight editor windows in GameLogic drive it,
// and three simulation files read its selection so a building can draw its
// editor handles. That was eleven allowlist entries, the largest cluster after
// Camera's. Measured surface:
// `grep -rhoE "g_locationEditor->[A-Za-z_]+" GameLogic`.
//
// The mode and tool fields were reached directly through the pointer, which a
// base class cannot offer, so they are accessors here. The fields themselves
// are unchanged and Species still uses them directly.
//
// LOCATION_EDITOR is a debug-build define; g_locationEditor is null in a build
// without it, and every caller below Species already checks.
class LocationEditorAccess
{
  public:
    virtual ~LocationEditorAccess() = default;

    // Declared here rather than in LocationEditor so that a caller naming a
    // mode or a tool does not need that header. LocationEditor derives from
    // this class, so the `LocationEditor::ModeBuilding` spelling still resolves
    // for the Species-side code that uses it.
    enum
    {
      ToolNone,
      ToolMove,
      ToolRotate,
      ToolLink,
      ToolCreate,
      ToolNumTypes
    };

    enum
    {
      ModeNone,        // 0
      ModeLandTile,    // 1
      ModeLandFlat,    // 2
      ModeBuilding,    // 3
      ModeLight,       // 4
      ModeInstantUnit, // 5
      ModeCameraMount, // 6
      ModeNumModes
    };

    virtual void RequestMode(int _mode) = 0;

    // Non-const to match LocationEditor's existing declaration.
    virtual int GetMode() = 0;

    // -1 when nothing is selected.
    virtual int GetSelectionId() const = 0;
    virtual void SetSelectionId(int _id) = 0;

    // ToolNone when nothing is selected.
    virtual int GetTool() const = 0;
    virtual void SetTool(int _tool) = 0;

    // A POINTER, because its one caller binds it to an editor value control
    // that writes through the address. An accessor returning by value cannot
    // stand in for taking the address of a field.
    virtual int* MoveBuildingsWithLandscapeField() = 0;
};
