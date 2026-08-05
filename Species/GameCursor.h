#pragma once

#include <memory>
#include <vector>

/*
    Renders all mouse cursors of any kind in-game.
    Responsible for figuring out which mouse cursor to render.
    Also renders selection arrows to inform the player which units are selected

 */


#include "GameCursorAccess.h"
#include "RgbColour.h"
#include "WorldObject.h"



// ============================================================================


namespace Species
{
  class MouseCursor;
  class MouseCursorMarker;

  class GameCursor : public GameCursorAccess
  {
    protected:
      // All eight owning. m_cursorMissile is the only one that starts empty and
      // is assigned later.
      std::unique_ptr<MouseCursor> m_cursorStandard;
      std::unique_ptr<MouseCursor> m_cursorPlacement;
      std::unique_ptr<MouseCursor> m_cursorHighlight;
      std::unique_ptr<MouseCursor> m_cursorSelection;
      std::unique_ptr<MouseCursor> m_cursorMoveHere;
      std::unique_ptr<MouseCursor> m_cursorDisabled;
      std::unique_ptr<MouseCursor> m_cursorMissile;
      std::unique_ptr<MouseCursor> m_cursorTurretTarget;

      std::string m_selectionArrowFilename;
      std::string m_selectionArrowShadowFilename;

      // The shadow name is always "shadow_" + the main one, and keeping the two
      // in step by hand is what the pair of formatted writes here used to do.
      void SetArrowFilenames(std::string_view _mainFilename);
      float m_selectionArrowBoost;

      bool m_highlightingSomething;
      bool m_validPlacementOpportunity;
      bool m_moveableEntitySelected;

      bool GetSelectedObject(WorldObjectId& _id, DirectX::XMFLOAT3& _pos);
      bool GetHighlightedObject(WorldObjectId& _id, DirectX::XMFLOAT3& _pos, float& _radius);

      void RenderSelectionArrows(WorldObjectId _id, DirectX::XMFLOAT3 const& _pos);
      void RenderSelectionArrow(float _screenX, float _screenY, float _screenDX, float _screenDY, float _size, float _alpha);

      void RenderMarkers();
      void FindScreenEdge(DirectX::XMFLOAT2 const& _line, float* _posX, float* _posY);

      std::vector<std::unique_ptr<MouseCursorMarker>> m_markers;

    public:
    public:
      GameCursor();
      ~GameCursor();

      void CreateMarker(DirectX::XMFLOAT3 const& _pos) override;
      void BoostSelectionArrows(float _seconds); // Show selection arrows for a specified time
      void Render();

      void RenderStandardCursor(float _screenX, float _screenY);
      void RenderWeaponMarker(DirectX::XMFLOAT3 _pos, DirectX::XMFLOAT3 _front, DirectX::XMFLOAT3 _up);

      bool AdviseHighlightingSomething() { return m_highlightingSomething; };
      bool AdvisePlacementOpportunity() { return m_validPlacementOpportunity; };
      bool AdviseMoveableEntitySelected() { return m_moveableEntitySelected; };
  };


  // ============================================================================

  class MouseCursorMarker
  {
    public:
      // Braced to zero, as Vector3's default constructor did: RenderMarkers
      // reads all three, and CreateMarker is the only thing that writes them.
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_front{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_up{0.0f, 0.0f, 0.0f};
      float m_startTime;
  };


  // ============================================================================

  class MouseCursor
  {
    protected:
      char* m_mainFilename;
      char* m_shadowFilename;

      float m_size;
      bool m_animating;
      bool m_shadowed;
      float m_hotspotX; // 0,0 means hotspot is in top left of bitmap
      float m_hotspotY; // 1,1 means hotspot is in bottom right of bitmap
      RGBAColour m_colour;

      float GetSize();

    public:
      MouseCursor(char const* _filename);
      ~MouseCursor();

      void SetSize(float _size);
      void SetAnimation(bool _onOrOff);
      void SetShadowed(bool _onOrOff);
      void SetHotspot(float x, float y);
      void SetColour(RGBAColour const& _col);

      void Render(float _x, float _y);
      void Render3D(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up, bool _cameraScale = true);
  };
} // namespace Species
