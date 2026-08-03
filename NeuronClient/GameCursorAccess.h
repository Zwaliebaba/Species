#pragma once

#include "Vector3.h"

// What the layers below Species ask the game cursor to do.
//
// GameCursor lives in Species and was reached as g_app->m_gameCursor, which is
// one of the last things keeping App.h in the world code that
// tasks/layering-inversion.yaml T15 moves down. Measured surface:
// `grep -rhoE "m_gameCursor->[A-Za-z_]+"` finds these seven and nothing else.
//
// Unlike the earlier seams there is no TheGameCursor(): every caller in the
// tree uses one of these, so the interface is the whole used API and Species
// has no reason to reach past it.
class GameCursorAccess
{
  public:
    virtual ~GameCursorAccess() = default;

    // A move order marker at a world position.
    virtual void CreateMarker(Vector3 const& _pos) = 0;

    // Show the selection arrows for a while, after a selection changes.
    virtual void BoostSelectionArrows(float _seconds) = 0;

    virtual void Render() = 0;
    virtual void RenderStandardCursor(float _screenX, float _screenY) = 0;

    // What the cursor is currently over, read by the control-help system to
    // decide which prompt to show.
    virtual bool AdviseHighlightingSomething() = 0;
    virtual bool AdvisePlacementOpportunity() = 0;
    virtual bool AdviseMoveableEntitySelected() = 0;
};
