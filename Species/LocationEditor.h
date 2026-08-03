#pragma once

#ifdef LOCATION_EDITOR

#include "LocationEditorAccess.h"
#include "Vector3.h"
#include "WorldPointers.h"


class InstantUnit;
class LandscapeTile;


class LocationEditor : public LocationEditorAccess
{
  public:
    int m_landscapeGrabX;
    int m_landscapeGrabZ;
    int m_newLandscapeX; // Used for landscape dragging
    int m_newLandscapeZ;
    int m_mode;

    int m_moveBuildingsWithLandscape;


  private:
    bool m_waitingForRelease;

    void CreateEditWindowForMode(int _mode);

    int DoesRayHitBuilding(Vector3 const& rayStart, Vector3 const& rayDir);
    int DoesRayHitInstantUnit(Vector3 const& rayStart, Vector3 const& rayDir);
    int DoesRayHitCameraMount(Vector3 const& rayStart, Vector3 const& rayDir);
    int IsPosInLandTile(Vector3 const& pos);
    int IsPosInFlattenArea(Vector3 const& pos);

    void AdvanceModeNone();
    void AdvanceModeLandTile();
    void AdvanceModeLandFlat();
    void AdvanceModeBuilding();
    void AdvanceModeLight();
    void AdvanceModeInstantUnit();
    void AdvanceModeCameraMount();

    void RenderUnit(InstantUnit* _iu);

    void RenderModeLandTile();
    void RenderModeLandFlat();
    void RenderModeBuilding();
    void RenderModeInstantUnit();
    void RenderModeCameraMount();

    void MoveBuildingsInTile(LandscapeTile* _tile, float _dX, float _dZ);

  public:
    int m_tool;           // == TypeNone if no object selected
    int m_selectionId;    // == -1 if no object selected
    int m_subSelectionId; // Some modes need a second selection ID, eg ModeRoute
                          // where one ID specifies the route, the other the way
                          // point within that route.

    LocationEditor();
    ~LocationEditor();

    void RequestMode(int _mode);
    int GetMode();

    // The fields below were reached directly through g_locationEditor from
    // GameLogic. A base class cannot offer a field, so the interface offers
    // these; the fields themselves are unchanged.
    int GetSelectionId() const { return m_selectionId; }
    void SetSelectionId(int _id) { m_selectionId = _id; }
    int GetTool() const { return m_tool; }
    void SetTool(int _tool) { m_tool = _tool; }
    int* MoveBuildingsWithLandscapeField() { return &m_moveBuildingsWithLandscape; }

    void Advance();
    void Render();
};


// g_locationEditor is a LocationEditorAccess* so the layers below Species need
// only the interface. Species reaches the whole class through here, at every
// call site. The cast is safe because Main is the only thing that assigns it,
// and it assigns a LocationEditor or null.
inline LocationEditor* TheLocationEditor() { return static_cast<LocationEditor*>(g_locationEditor); }


#endif // LOCATION_EDITOR
