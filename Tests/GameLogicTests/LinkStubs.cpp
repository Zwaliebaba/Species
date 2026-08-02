#include "pch.h"

// Symbols that GameLogic references but does not own.
//
// GameLogic reaches up into the Species executable — 586 includes, the largest
// violation cluster in the tree (see AGENTS.md, Layering). Those upward
// references are harmless while everything is linked into one binary, because
// Species.exe supplies them. A test DLL links GameLogic.lib *without* the
// executable, so every one of them becomes an unresolved external, and pulling
// in one object file drags in whatever globals it happens to touch:
//
//   GameLogic.lib(WorldObject.obj) : error LNK2001:
//       unresolved external symbol "class App * g_app"
//
// This file defines the minimum needed to link, so the parts of GameLogic that
// do not actually use them at runtime can be tested now rather than after the
// layering work lands.
//
// **This list may only shrink.** It is the same rule as
// tools/layering_allowlist.txt and for the same reason: every entry is a
// dependency that should not exist, and adding one hides the problem instead of
// recording it. If a test needs a new stub, the honest options are to test
// something that does not reach upward, or to do the layering task that removes
// the reference. When migration stage 7 (globals) lands, this file goes away —
// it is scaffolding around a known defect, not a test fixture.
//
// Every stub is deliberately null. A test that reaches one crashes on the spot,
// in the test that did it, rather than quietly running against a fake world.

class App;

App* g_app = nullptr;
