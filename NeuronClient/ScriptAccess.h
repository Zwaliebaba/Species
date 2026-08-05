#pragma once

// What the layers below Species ask the cutscene script system to do.
//
// Script lives in Species, so the four GameLogic files that start, skip or
// query a script had to include Script.h upward. This is the surface they
// actually use, measured rather than guessed:
// `grep -rhoE "g_script->[A-Za-z_]+" GameLogic NeuronClient` finds exactly
// these three.
//
// The seam is the pointer's TYPE rather than a second global, as in
// CameraAccess.h and RendererAccess.h: g_script in WorldPointers.h is a
// ScriptAccess*, so dereferencing it needs only this header. Species reaches
// the rest of the API — Advance, the op-code table, m_permitEscape — through
// TheScript() in Script.h.
//
// Species goes through TheScript() at EVERY call site, not only the ones
// needing a member this interface omits. Camera's seam learned that the hard
// way: an overload set that is complete on the concrete type and partial on
// the interface breaks whichever call sites happen to use the missing
// overloads, and splitting call sites by that is a trap.


namespace Neuron
{
  class ScriptAccess
  {
    public:
      virtual ~ScriptAccess() = default;

      virtual void RunScript(char const* _filename) = 0;
      virtual bool IsRunningScript() = 0;
      virtual bool Skip() = 0;
  };
} // namespace Neuron
