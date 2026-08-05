
DEF_CONTROL_TYPE(MenuLeft, InputType::INPUT_TYPE_BOOL)     // Select button to the left
DEF_CONTROL_TYPE(MenuRight, InputType::INPUT_TYPE_BOOL)    // Select button to the right
DEF_CONTROL_TYPE(MenuUp, InputType::INPUT_TYPE_BOOL)       // Select button above
DEF_CONTROL_TYPE(MenuDown, InputType::INPUT_TYPE_BOOL)     // Select button below
DEF_CONTROL_TYPE(MenuActivate, InputType::INPUT_TYPE_BOOL) // Press menu button
DEF_CONTROL_TYPE(MenuEscape, InputType::INPUT_TYPE_BOOL)   // Escape out of this menu
DEF_CONTROL_TYPE(MenuClose, InputType::INPUT_TYPE_BOOL)    // close current window

// DEF_CONTROL_TYPE( ChatLogDisplay,                    INPUT_TYPE_BOOL ) // Show chat log
// DEF_CONTROL_TYPE( ChatLogHide,                       INPUT_TYPE_BOOL ) // Hide chat log
DEF_CONTROL_TYPE(SkipMessage, InputType::INPUT_TYPE_BOOL) // Skip chat message
DEF_CONTROL_TYPE(SkipCutscene, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(CameraLeft, InputType::INPUT_TYPE_BOOL)      // Move camera left (boolean)
DEF_CONTROL_TYPE(CameraRight, InputType::INPUT_TYPE_BOOL)     // Move camera right (boolean)
DEF_CONTROL_TYPE(CameraUp, InputType::INPUT_TYPE_BOOL)        // Move camera up (boolean)
DEF_CONTROL_TYPE(CameraDown, InputType::INPUT_TYPE_BOOL)      // Move camera down (boolean)
DEF_CONTROL_TYPE(CameraForwards, InputType::INPUT_TYPE_BOOL)  // Move camera forwards (boolean)
DEF_CONTROL_TYPE(CameraBackwards, InputType::INPUT_TYPE_BOOL) // Move camera backwards (boolean)
DEF_CONTROL_TYPE(CameraZoom, InputType::INPUT_TYPE_BOOL)      // Zoom camera in (boolean)
DEF_CONTROL_TYPE(CameraDeZoom, InputType::INPUT_TYPE_BOOL)    // Zoom camera out (boolean)

DEF_CONTROL_TYPE(CameraMove, InputType::INPUT_TYPE_2D)          // Analog camera control
DEF_CONTROL_TYPE(TargetMove, InputType::INPUT_TYPE_2D)          // Analog target control
DEF_CONTROL_TYPE(TargetMoveSecondary, InputType::INPUT_TYPE_2D) // Analog target control
DEF_CONTROL_TYPE(TargetMoveZ, InputType::INPUT_TYPE_1D)         // Analog target control
DEF_CONTROL_TYPE(CameraZoomChange, InputType::INPUT_TYPE_1D)    // Analog camera zoom
DEF_CONTROL_TYPE(CameraSpeedup, InputType::INPUT_TYPE_BOOL)     // Hold to go faster
DEF_CONTROL_TYPE(CameraSlowdown, InputType::INPUT_TYPE_BOOL)    // Hold to go slower
DEF_CONTROL_TYPE(CameraRotate, InputType::INPUT_TYPE_BOOL)      // rotate the cam in entity track mode
DEF_CONTROL_TYPE(CameraRotateLeft, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(CameraRotateRight, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(CameraSwitchMode, InputType::INPUT_TYPE_BOOL) // Switch from free to track cam

DEF_CONTROL_TYPE(CameraDebugRotate, InputType::INPUT_TYPE_BOOL) // Hold for debug rotate mode
DEF_CONTROL_TYPE(GestureActive, InputType::INPUT_TYPE_BOOL)     // Hold to write a gesture

DEF_CONTROL_TYPE(UnitDeselect, InputType::INPUT_TYPE_BOOL)   // Escape unit control mode
DEF_CONTROL_TYPE(UnitCycleLeft, InputType::INPUT_TYPE_BOOL)  // Cycle left through tasks
DEF_CONTROL_TYPE(UnitCycleRight, InputType::INPUT_TYPE_BOOL) // Cycle right through tasks

DEF_CONTROL_TYPE(UnitEndSetTarget, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(UnitSetTarget, InputType::INPUT_TYPE_BOOL) // "Click to go to target"
DEF_CONTROL_TYPE(UnitMove, InputType::INPUT_TYPE_2D)        // Analog unit movement
DEF_CONTROL_TYPE(UnitMoveDirectionChange, InputType::INPUT_TYPE_2D)
DEF_CONTROL_TYPE(UnitPrimaryFireTarget, InputType::INPUT_TYPE_BOOL)   // "Press button to fire at target"
DEF_CONTROL_TYPE(UnitPrimaryFireDirected, InputType::INPUT_TYPE_2D)   // "Move joystick to fire"
DEF_CONTROL_TYPE(UnitSecondaryFireTarget, InputType::INPUT_TYPE_BOOL) // "Press button to fire at target"
DEF_CONTROL_TYPE(UnitSecondaryAim, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(UnitStartSecondaryFireDirected, InputType::INPUT_TYPE_2D) // Start moving power arc
DEF_CONTROL_TYPE(UnitSecondaryFireDirected, InputType::INPUT_TYPE_2D)      // Fire wherever power arc is

DEF_CONTROL_TYPE(UnitCreate, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(WeaponCycleLeft, InputType::INPUT_TYPE_BOOL)  // Cycle left through weapons or unit modes
DEF_CONTROL_TYPE(WeaponCycleRight, InputType::INPUT_TYPE_BOOL) // Cycle right through weapons or unit modes

DEF_CONTROL_TYPE(TaskManagerSelectNextTask, InputType::INPUT_TYPE_BOOL) // Select Next active task
DEF_CONTROL_TYPE(TaskManagerDisplay, InputType::INPUT_TYPE_BOOL)        // Show the task manager
DEF_CONTROL_TYPE(TaskManagerHide, InputType::INPUT_TYPE_BOOL)           // Hide the task manager
DEF_CONTROL_TYPE(TaskManagerSwitchTask, InputType::INPUT_TYPE_BOOL)     // Switch task
DEF_CONTROL_TYPE(TaskManagerEndTask, InputType::INPUT_TYPE_BOOL)        // Kill task

DEF_CONTROL_TYPE(GesturesTaskManagerDisplay, InputType::INPUT_TYPE_BOOL) // Show the gestural task manager
DEF_CONTROL_TYPE(GesturesTaskManagerHide, InputType::INPUT_TYPE_BOOL)    // Hide the gestural task manager
DEF_CONTROL_TYPE(GesturesTaskManagerEndTask, InputType::INPUT_TYPE_BOOL) // Kill task
DEF_CONTROL_TYPE(GesturesChatLog, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(GesturesSwitchUnit, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(GestureLeft, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(GestureRight, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(IconsTaskManagerDisplay, InputType::INPUT_TYPE_BOOL)        // Show the iconic task manager
DEF_CONTROL_TYPE(IconsTaskManagerDisplayDown, InputType::INPUT_TYPE_BOOL)    // When icon task manager key has been pressed down
DEF_CONTROL_TYPE(IconsTaskManagerDisplayPressed, InputType::INPUT_TYPE_BOOL) // When icon task manager key is pressed
DEF_CONTROL_TYPE(IconsTaskManagerDisplayUp, InputType::INPUT_TYPE_BOOL)      // When icon task manager key is released
DEF_CONTROL_TYPE(IconsTaskManagerHide, InputType::INPUT_TYPE_BOOL)           // Hide the iconic task manager
DEF_CONTROL_TYPE(IconsTaskManagerEndTask, InputType::INPUT_TYPE_BOOL)        // Kill task
DEF_CONTROL_TYPE(IconsChatLog, InputType::INPUT_TYPE_BOOL)                   // Show chat log

DEF_CONTROL_TYPE(ActivateTMButton, InputType::INPUT_TYPE_BOOL) // Activate Task Manager buttons

// DEF_CONTROL_TYPE( EclipseMouseMove,                  INPUT_TYPE_2D    ) // Eclipse thinks the mouse is moving
DEF_CONTROL_TYPE(EclipseLMouseDown, InputType::INPUT_TYPE_BOOL)    // Eclipse left mouse button down
DEF_CONTROL_TYPE(EclipseLMousePressed, InputType::INPUT_TYPE_BOOL) // Eclipse left mouse button is pressed
DEF_CONTROL_TYPE(EclipseRMousePressed, InputType::INPUT_TYPE_BOOL) // Eclipse right mouse button is pressed
DEF_CONTROL_TYPE(ScrollSpeedup, InputType::INPUT_TYPE_BOOL)        // Hold to scroll faster

DEF_CONTROL_TYPE(SelectLocation, InputType::INPUT_TYPE_BOOL)     // Select a location on the world screen
DEF_CONTROL_TYPE(LocationDragActive, InputType::INPUT_TYPE_BOOL) // Hold to drag a location
DEF_CONTROL_TYPE(DeselectLocation, InputType::INPUT_TYPE_BOOL)   // Deselect a location after dragging

DEF_CONTROL_TYPE(TileSelect, InputType::INPUT_TYPE_BOOL) // Location editor tile select
DEF_CONTROL_TYPE(TileDrop, InputType::INPUT_TYPE_BOOL)   // Location editor tile drop
DEF_CONTROL_TYPE(TileDrag, InputType::INPUT_TYPE_BOOL)   // Location editor tile drag

DEF_CONTROL_TYPE(FileMultiSelect, InputType::INPUT_TYPE_BOOL) // Hold to select multiple files

DEF_CONTROL_TYPE(DebugDropPacket, InputType::INPUT_TYPE_BOOL) // For stress testing the network system

DEF_CONTROL_TYPE(TaskManagerSelectTask1, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 0
DEF_CONTROL_TYPE(TaskManagerSelectTask2, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 1
DEF_CONTROL_TYPE(TaskManagerSelectTask3, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 2
DEF_CONTROL_TYPE(TaskManagerSelectTask4, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 3
DEF_CONTROL_TYPE(TaskManagerSelectTask5, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 4
DEF_CONTROL_TYPE(TaskManagerSelectTask6, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 5
DEF_CONTROL_TYPE(TaskManagerSelectTask7, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 6
DEF_CONTROL_TYPE(TaskManagerSelectTask8, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 7
DEF_CONTROL_TYPE(TaskManagerSelectTask9, InputType::INPUT_TYPE_BOOL) // Go to task manager screenzone 8

DEF_CONTROL_TYPE(IconsTaskManagerNewSquad, InputType::INPUT_TYPE_BOOL)    // Make a new Insertion Squad
DEF_CONTROL_TYPE(IconsTaskManagerNewEngineer, InputType::INPUT_TYPE_BOOL) // Make a new Engineer
DEF_CONTROL_TYPE(IconsTaskManagerNewOfficer, InputType::INPUT_TYPE_BOOL)  // Make a new Officer
DEF_CONTROL_TYPE(IconsTaskManagerNewArmour, InputType::INPUT_TYPE_BOOL)   // Make a new Armour

DEF_CONTROL_TYPE(IconsTaskManagerSelectGrenade, InputType::INPUT_TYPE_BOOL)    // Select Grenade as the current secondary weapon
DEF_CONTROL_TYPE(IconsTaskManagerSelectRocket, InputType::INPUT_TYPE_BOOL)     // Select Rocket as the current secondary weapon
DEF_CONTROL_TYPE(IconsTaskManagerSelectAirStrike, InputType::INPUT_TYPE_BOOL)  // Select AirStrike as the current secondary weapon
DEF_CONTROL_TYPE(IconsTaskManagerSelectController, InputType::INPUT_TYPE_BOOL) // Select Controller as the current secondary weapon

DEF_CONTROL_TYPE(LoaderHelp, InputType::INPUT_TYPE_BOOL)    // Hold for a help screen
DEF_CONTROL_TYPE(SepulvedaHelp, InputType::INPUT_TYPE_BOOL) // Activate Sepulveda

DEF_CONTROL_TYPE(GOLLoaderSpeedup, InputType::INPUT_TYPE_BOOL) // Game of Life Loader keys
DEF_CONTROL_TYPE(GOLLoaderSlowdown, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(GOLLoaderReset, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(RTLoaderReflectivenessIncrease, InputType::INPUT_TYPE_BOOL) // Ray Tracer Loader keys
DEF_CONTROL_TYPE(RTLoaderReflectivenessDecrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderPixelWaveIncrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderPixelWaveDecrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderMotionBlurIncrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderMotionBlurDecrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderPixelBlurIncrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderPixelBlurDecrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderBrightnessIncrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderBrightnessDecrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderResolutionIncrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderResolutionDecrease, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderCameraMode1, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderCameraMode2, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderCameraMode3, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderLMB, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(RTLoaderRMB, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(StartAttractMode, InputType::INPUT_TYPE_BOOL) // Activate Attract Mode
DEF_CONTROL_TYPE(StopAttractMode, InputType::INPUT_TYPE_BOOL)  // Deactivate Attract Mode

DEF_CONTROL_TYPE(CameraFreeMovement, InputType::INPUT_TYPE_BOOL) // Change to free from first person

DEF_CONTROL_TYPE(DebugCameraFixUp, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(SkipTutorial, InputType::INPUT_TYPE_BOOL)

DEF_CONTROL_TYPE(GamePause, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(ToggleEditor, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(CreateScreenShot, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(ToggleCheatMenu, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(MouseMovement, InputType::INPUT_TYPE_2D)
DEF_CONTROL_TYPE(IconsTaskManagerQuickUnit, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(IconsTaskManagerQuickUnitCreate, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(IconsTaskManagerQuickUnitLeft, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(IconsTaskManagerQuickUnitRight, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(ControllerUnplugged, InputType::INPUT_TYPE_BOOL)
DEF_CONTROL_TYPE(ControllerPlugged, InputType::INPUT_TYPE_BOOL)
