#pragma once

// What the layers below Species tell the control-help system.
//
// ControlHelpSystem lives in Species and shows the on-screen prompts for the
// controls the player has not used yet. The target cursor is the only thing
// below Species that talks to it, and it reports one condition as used.
// Measured surface:
// `grep -rhoE "g_controlHelpSystem->[A-Za-z_]+" GameLogic NeuronClient`.
//
// The condition enum moves here rather than being duplicated, so that a caller
// naming one does not need ControlHelp.h. ControlHelpSystem derives from this
// class, so `ControlHelpSystem::CondCameraAim` and the MaxConditions array
// bounds still resolve for the Species-side code that spells them that way.
class ControlHelpAccess
{
  public:
    virtual ~ControlHelpAccess() = default;

    enum
    {
      CondDestroyUnit,
      CondTaskManagerCreateBlue,
      CondTaskManagerCloseBlue,
      CondTaskManagerCloseRed,
      CondDeselectUnit,
      CondMoveUnit,
      CondSelectUnit,
      CondTaskManagerCreateGreen,
      CondTaskManagerSelect,
      CondPlaceUnit,
      CondPromoteOfficer,
      CondMoveCameraOrUnit,
      CondZoom,
      CondCameraAim,
      CondSquaddieFire,
      CondChangeWeapon,
      CondChangeOrders,
      CondCameraUp,
      CondCameraDown,
      CondFireGrenades,
      CondFireRocket,
      CondFireAirstrike,
      CondOfficerSetGoto,
      CondOfficerSetFollow,
      CondArmourSetTurret,
      CondSwitchPrevUnit,
      CondSwitchNextUnit,
      CondRadarAim,
      CondSkipCutscene,
      MaxConditions
    };

    virtual void RecordCondUsed(int _cond) = 0;
};
