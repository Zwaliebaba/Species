#include "pch.h"

// Symbols that GameLogic references but does not own.
//
// **This file is now empty, and that is the point.** It used to define
// `App* g_app = nullptr;`, because a test DLL links GameLogic.lib without the
// executable that owns the singleton, so pulling in one object file dragged in
// whatever App members it happened to touch:
//
//   GameLogic.lib(WorldObject.obj) : error LNK2001:
//       unresolved external symbol "class App * g_app"
//
// tasks/layering-inversion.yaml T8 and T14 removed the reason. The subsystem
// pointers moved to NeuronClient/WorldPointers.h, the application state to
// AppState.h, and the seven actions only App can perform now go through the
// AppCommands interface that App installs at startup. All three live in
// NeuronClient, which the test DLL already links, so nothing needs stubbing —
// GameLogic no longer names a symbol the executable owns.
//
// The rule that got us here still stands: **this list may only shrink.** It is
// the same rule as tools/layering_allowlist.txt and for the same reason. If a
// test seems to need a new stub, the honest options are to test something that
// does not reach upward, or to do the layering task that removes the reference.
//
// The file itself stays only until GameLogicTests grows tests that prove the
// link works without it (T11); at that point it can be deleted outright.
