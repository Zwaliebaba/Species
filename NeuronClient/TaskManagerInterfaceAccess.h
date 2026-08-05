#pragma once

// What the layers below Species ask the task manager's interface to do.
//
// TaskManagerInterface lives in Species; the research item posts messages to
// it, the rocket hides it, and the target cursor asks whether it is showing.
// Measured surface:
// `grep -rhoE "g_taskManagerInterface->[A-Za-z_]+" GameLogic NeuronClient`.
//
// m_visible was read directly through the pointer, which a base class cannot
// offer; it is IsVisible() here. The field is unchanged and Species still
// writes it directly.


namespace Neuron
{
  class TaskManagerInterfaceAccess
  {
    public:
      virtual ~TaskManagerInterfaceAccess() = default;

      // Declared here rather than in TaskManagerInterface so that a caller
      // naming a message does not need that header. TaskManagerInterface derives
      // from this class, so `TaskManagerInterface::MessageResearch` still
      // resolves for the Species-side code that spells it that way.
      enum
      {
        MessageSuccess,
        MessageFailure,
        MessageShutdown,
        MessageResearch,
        MessageResearchUpgrade,
        MessageObjectivesComplete,
        NumMessages
      };

      virtual void SetCurrentMessage(int _messageType, int _taskType, float _timer) = 0;

      // TaskManagerInterface's own declaration defaults _visible to true. The
      // default is deliberately not repeated: a call through this interface
      // passes the argument, which is what the one caller below Species does.
      // Camera's seam is where that lesson came from.
      virtual void SetVisible(bool _visible) = 0;

      virtual bool IsVisible() const = 0;
  };
} // namespace Neuron
