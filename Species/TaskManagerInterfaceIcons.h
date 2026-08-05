#pragma once

#include <memory>
#include <vector>

#include "Entity.h"
#include "WorldObject.h"
#include "TaskManagerInterface.h"


namespace Species
{
  class QuickUnitButton;

  class TaskManagerInterfaceIcons : public TaskManagerInterface
  {
    protected:
      enum
      {
        ScreenOverlay, // On all screens
        ScreenTaskManager,
        ScreenObjectives,
        ScreenResearch
      };
      int m_screenId;

      float m_screenY;
      float m_desiredScreenY;
      float m_screenW;
      float m_screenH;

      float m_chatLogY;

      // Both owning. A zone moves from m_newScreenZones to m_screenZones once a
      // frame; the transfer is a move, and the old m_screenZones contents are
      // destroyed at the same point EmptyAndDelete used to destroy them.
      std::vector<std::unique_ptr<ScreenZone>> m_screenZones;    // All clickable areas on-screen
      std::vector<std::unique_ptr<ScreenZone>> m_newScreenZones; // New zones generated this frame
      std::vector<KeyboardShortcut*> m_keyboardShortcuts;        // Keyboard shortcuts to screenzones
      int m_currentScreenZone;
      int m_currentMouseScreenZone;
      int m_currentScrollZone;
      float m_screenZoneTimer;
      double m_taskManagerDownTime;

    public:
      int m_currentQuickUnit;
      int m_quickUnitDirection;
      std::vector<std::unique_ptr<QuickUnitButton>> m_quickUnitButtons;

    protected:
      void AdvanceScrolling();
      void AdvanceScreenEdges();
      void AdvanceScreenZones();
      void AdvanceKeyboardShortcuts();
      void AdvanceTerminate();
      void AdvanceQuickUnit();

      void HideTaskManager();

      bool ScreenZoneHighlighted(ScreenZone* _zone);
      void RunScreenZone(const char* _name, int _data); // Occurs during click or keypress

      void SetupRenderMatrices(int _screenId);
      void ConvertMousePosition(float& _x, float& _y);
      void RestoreRenderMatrices();

      void RenderTargetAreas();
      void RenderMessages();
      void RenderRunningTasks();
      void RenderOverview();
      void RenderTaskManager();
      void RenderObjectives();
      void RenderResearch();
      void RenderTitleBar();
      void RenderScreenZones();
      void RenderTooltip();
      void RenderCreateTaskMenu();
      void RenderQuickUnit();

      void RenderCompass(float _screenX, float _screenY, DirectX::XMFLOAT3 const& _worldPos, bool selected, float _size);

      int GetQuickUnitTask(int _position = 2);
      void CreateQuickUnitInterface();
      void DestroyQuickUnitInterface();

      bool ButtonHeld();
      bool ButtonHeldAndReleased();

    public:
      TaskManagerInterfaceIcons();

      virtual void Advance();
      virtual void Render();

      bool ControlEvent(TMControl _type);
      bool AdviseCreateControlHelpBlue();
      bool AdviseCreateControlHelpGreen();
      bool AdviseCloseControlHelp();
      bool AdviseOverSelectableZone();
  };

  class QuickUnitButton
  {
    public:
      int m_taskId;
      int m_positionId;
      float m_size;

      int m_x;
      int m_y;
      float m_alpha;

      bool m_movable;

    public:
      QuickUnitButton();
      void Advance();
      void Render();
      void ActivateButton();
  };
} // namespace Species
