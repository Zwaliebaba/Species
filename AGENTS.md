# AGENTS.md

Orientation for anyone — human or agent — making changes to Species.

Read this first. It tells you what the project is, what state it is in, what the
current priority is, and what you must run before pushing. The details live in
linked documents; this file is the map.

| I need to know… | Read |
|---|---|
| How to build and run it | [`docs/BUILD.md`](docs/BUILD.md) |
| How the layers fit together | [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) |
| How to write code that fits | [`CODING_STANDARDS.md`](CODING_STANDARDS.md) |
| **What to test, and where the test goes** | [`docs/TESTING.md`](docs/TESTING.md) |
| How work is broken down | [`docs/TASK_DAG.md`](docs/TASK_DAG.md) |
| What a Spirit / Officer / Trunk Port *is* | [`docs/GLOSSARY.md`](docs/GLOSSARY.md) |

---

## What this is

Species is a C++23 game built on the **Neuron** engine. The long-term goal is a
large-scale realtime multiplayer world in which player colonies work, live and
survive persistently.

It is not that yet. The codebase began as the Darwinia source and is partway
through being reshaped: renamed, relayered, and modernised into something a
persistent authoritative server can be built on. Roughly 113,000 lines of C++
across six MSBuild projects. It links only against the OS (OpenGL, GLU,
XAudio2, Winsock) and takes one header-only dependency, **DirectXMath**,
which ships in the Windows SDK — no library to link and nothing vendored.
`tasks/Archive/directxmath-migration.yaml` replaced the inherited hand-rolled math with
it and then deleted it: there is no Neuron vector or matrix type, storage is
`XMFLOAT2/3`, `XMFLOAT3X3` and `XMFLOAT4X4`, and `NeuronCore/NeuronMath.h`
holds the conventions rather than a class.

**Do not treat the ambition as the current scope.** The realtime world does not
exist. Nothing in the tree is designed for it yet. Work toward it happens by
making the existing code capable of supporting it, not by building it alongside.

---

## Current priority

**Cleanup and modernisation first — and as of 2026-08-05 THE PLANNED
MODERNISATION IS FINISHED.** Eleven plans, 147 tasks, every one done or
deliberately abandoned, all of them in `tasks/Archive/`.

**THREE PLANS OPENED ON 2026-08-06 on the owner's direction** — the first work
since the modernisation that is not cleanup. One of them, `sound-xaudio2`,
closed the same day at 11 of 11 and is in
[`tasks/Archive/sound-xaudio2.yaml`](tasks/Archive/sound-xaudio2.yaml): one
native XAudio2 backend with X3DAudio positioning and the effects on XAudio2,
and DirectSound, the software mixer, the waveOut layer and the `SoundLibrary`
preference all deleted. Two remain open:

| Plan | What it does | State |
|---|---|---|
| [`tasks/input-native-events.yaml`](tasks/input-native-events.yaml) | Native input becomes an event stream with a UI-first consuming router and a subscription API; `WM_CHAR` text, Raw Input mouse | **12 of 13 — all code done, T12 is the owner's Garden run** |
| [`tasks/network-transport.yaml`](tasks/network-transport.yaml) | Bounded wire reads, one polled socket replacing both listener threads, a loopback test seam, server-assigned identity, liveness | 0 of 12 |

**`input-native-events` has one node left and it is not code.** T12 is a
seven-step Garden run on a build carrying T1–T11, and **nothing in that plan has
ever been in front of a running game** — see *What working looks like*. It is
the kind of change a green build says least about: it changes what happens on a
keystroke, on a click over a menu, on focus loss and on every frame of camera
aim, and no test in the tree renders a menu or turns a camera.

`python3 tools/check_task_dag.py --next tasks/<plan>.yaml` is the authority on
what can be started; read it rather than this table, which will go stale.

That converted the inherited Darwinia code into Neuron-style C++23 with enforced
layer boundaries: zero upward includes, standard containers, `std::string` and
`std::format`, `std::unique_ptr` and values, scoped enums, two namespaces, and
DirectXMath as the only math. A runnable game and the authoritative world server
were always the later milestones, and this is what they were waiting on.

**This does not mean the priority has changed by itself.** The owner sets what
comes next; nothing in the tree does. What that means for a task in front of you
today:

- **In scope, and now unblocked:** the work that was deliberately deferred while
  the foundation moved. `tasks/_openworld-prompt.md` is the one written-down
  candidate — it is a design prompt, not a plan, and it has not been run.
- **In scope, and unowned:** the leftovers the closed plans recorded rather than
  fixed. They are listed where they were found — the three raw-ownership
  survivors under *Ownership*, and under *Known issues* the unswept LCG sites,
  the untested entity and building behaviour, and the four `input-native-events`
  left, including the one that matters most: **a re-introduced controller has to
  restore the gamepad art the owner deleted, or the controller UI code has to go
  with it.** **None has a task.** Picking one up means writing a plan for it
  first; see *How work is broken down*.
- **Still true:** fixing things that are outright broken, when you encounter
  them in code you are already changing.
- **Still the rule that governs all of it:** anything larger than a single-file
  change is a DAG under `tasks/` before code is written. The modernisation
  finishing removes the plans, not the standard.

If a task you have been given falls outside this, say so before starting rather
than after.

### Definition of done

This phase ends when **`NeuronCore` and `NeuronServer` link into a headless
server that ticks without `NeuronClient`.** Concretely, all four of:

1. No upward include out of `NeuronCore`. **Met** — zero, down from 30, and
   `tools/check_layering.py` now rejects any upward include tree-wide.
2. `NeuronCore.vcxproj` lists no other project in `AdditionalIncludeDirectories`.
   **Met** — it lists none at all.
3. `Server.exe` links without `NeuronClient.lib` and advances sequence ids with
   no client attached. **Met** — `Server.vcxproj` references `NeuronCore` and
   `NeuronServer` only, and CI runs `Server.exe --ticks 20` on every push and
   fails if the sequence id has not advanced.
4. The build is green. **Met** — x64 Debug, every push.

**This phase is done.** `tasks/Archive/neuroncore-layering.yaml` is the plan that got
there; all thirteen of its tasks are complete.

What that does *not* mean: the game client runs, the world server exists, or
cross-architecture play works. It means the foundation no longer depends on the
things above it, so a server can be built without dragging a renderer in.

Deliberately *not* the exit criterion: the rest of the tree's upward includes,
and the client running. Both matter; neither gated the world server. The
allowlist stood at 628 when this phase ended and is **gone** —
`tasks/Archive/layering-inversion.yaml` took it to zero and deleted it.

> **Note:** the game runs again — first at `7ee8c00` (2026-08-02), most
> recently at `acf283b` (2026-08-05), all seven steps. That is recorded here because
> this file is where it gets recorded — see *What working looks like*. It does
> not lower the bar for a change: a successful compile is still not evidence that
> anything works, and most agents cannot launch the client at all. Report what
> you actually ran.

---

## Repository map

```
Species.slnx              Solution. ARM64 and x64, Debug and Release.
Directory.Build.props     Settings shared by every project. Defines $(SpeciesRoot).
GameData.targets          Stages GameData/ next to the executable after each build.
GameData/                 Game content: levels, shapes, textures, sounds, scripts.

NeuronCore/       ~4.8k   Foundation: sockets, threads, byte streams, the wire
                          protocol, filesystem, assertions, and the math
                          conventions in NeuronMath.h. It holds no math TYPES:
                          Vector2/3, Matrix33/34 and Plane were deleted, and
                          storage is DirectXMath's own. Static library.
                          PARTLY in namespace Neuron — see Namespaces below.
NeuronClient/     ~22k    Presentation: OpenGL renderer, sound, input drivers,
                          the Eclipse UI toolkit, resource loading. Static
                          library, all of it in namespace Neuron.
NeuronServer/     ~0.6k   Authoritative simulation host: Server, ServerToClient,
                          the client and team registries. Static library, all of
                          it in namespace Neuron.
GameLogic/        ~65k    Entities, buildings, teams, unit behaviour, in-game
                          windows, AND THE WHOLE WORLD MODEL — Location,
                          GlobalWorld, LevelFile, Landscape, Team, Unit, which
                          layering-inversion moved down out of the executable.
                          The bulk of the inherited code. Static library, all of
                          it in namespace Species.
Species/          ~16k    Client executable: app and main loop, camera, renderer
                          entry, task manager interface, location editor. NOT
                          the world model any more. In namespace Species except
                          WinMain.
Server/           ~0.1k   Headless server executable. Links NeuronCore and
                          NeuronServer only; ticks the host at 10 Hz.

Tests/            ~0.4k   One <Name>Tests project per library, on the Microsoft
                          Native Unit Test Framework. Built and run by CI.
tools/                    The checks CI runs. Run them locally too.
tasks/                    Task DAGs. See docs/TASK_DAG.md. EVERY plan is
                          finished and in tasks/Archive/; what is left here
                          is _template.yaml and the reading orders.
                          _next-batch.md is now a record rather than a
                          proposal — there is nothing left to schedule.
docs/                     Architecture, build, testing, glossary, task breakdown.
```

`NeuronServer` holds the authoritative host moved out of `NeuronCore`, and
`Server` is the binary that drives it — `Server.exe --ticks 20` runs the host for
two seconds and reports the sequence id it reached. Neither simulates a world
yet; the host sequences whatever clients send it, which is what it always did.

---

## Layering

The intended dependency direction, enforced by `tools/check_layering.py`:

```
NeuronCore                   no dependencies            namespace Neuron, in part
  NeuronClient               -> NeuronCore              namespace Neuron
  NeuronServer               -> NeuronCore              namespace Neuron
    GameLogic                -> NeuronCore, NeuronClient, NeuronServer
                                                        namespace Species
      Species  (exe)         -> GameLogic, NeuronClient, NeuronCore
                                                        namespace Species
      Server   (exe)         -> GameLogic, NeuronServer, NeuronCore
```

**Includes may only ever point downward, and the tree obeys this.** Zero upward
includes, down from the 628 inherited from Darwinia's single-binary layout.
`tasks/Archive/layering-inversion.yaml` removed them over eighteen tasks and deleted the
allowlist on the way out.

What went, and how: `App.h` — no file below `Species` includes it — the frame
clock, `Globals.h`, and then the subsystems behind `*Access` interfaces
(`Renderer`, `Camera`, `Script`, `UserInput`, `TaskManagerInterface`,
`ControlHelp`, `LocationEditor`, `GameCursor`, and the loaded world itself).
The world model — `Location`, `GlobalWorld`, `Team`, `Unit`, the grids, the
routing system, the landscape — moved down into `GameLogic` rather than being
reached up into.

**The check is strict and has no escape hatch.** If your change needs an upward
include, the design is wrong: move the shared declaration down into a layer both
sides can see, or invert the dependency behind an interface. Do not recreate the
allowlist.

`tasks/Archive/neuroncore-layering.yaml` is the plan that eliminated the `NeuronCore`
entries. All thirteen of its tasks are done — including T10, which dropped the
upward include paths from `NeuronCore.vcxproj` and made `Server.exe` tick.

---

## Namespaces

**Engine code is in `namespace Neuron`; game code is in `namespace Species`.**
`tasks/Archive/namespace-migration.yaml` finished this on 2026-08-05, all five
tasks. `docs/ARCHITECTURE.md#namespaces` has the detail; three facts belong
here because they change how you write a change:

1. **You do not qualify engine names.** `NeuronCore.h` ends with
   `using namespace Neuron;` and every project's pch includes it. That is why
   396 files moved into namespaces without a single call site changing.
2. **`NeuronCore` is only PARTLY in `Neuron`.** Its converted helpers —
   `FileSys`, `Debug`, `NeuronHelper`, `SlotMap`, `SliceWalker`, `LookupTable`,
   `VectorUtils` — are; the networking and protocol types, `Profiler` and
   `TeamControls` are not. "Is this a Neuron type?" is a per-type question in
   that layer.
3. **The game namespace has no using-directive and must not gain one.**
   `GameLogic` and the `Species` executable are its only code and both are
   inside it. The one exception is `Tests/GameLogicTests`, whose five
   game-touching sources each carry `using namespace Species;` — in the `.cpp`,
   never in `pch.h`.

**What a namespace change breaks is forward declarations, not call sites.** A
using-directive makes a name findable; it does not make `class Renderer;`
declare `Neuron::Renderer`. It declares a new `::Renderer`, and the error comes
out at LINK time pointing somewhere else. `check_layering.py` sees none of
this — a forward declaration includes nothing. Three more shapes cost a CI round
each and are written up in `docs/ARCHITECTURE.md`: a block-scope `extern` binds
to the GLOBAL namespace, a member of a global class cannot be defined inside a
namespace, and `void f(class Profiler*)` silently declares a new type.

---

## Ownership

**Migration stage 5 is COMPLETE.** `tasks/Archive/ownership.yaml` is the plan;
all eleven of its tasks are done, finishing 2026-08-05. Raw owning pointers
are `std::unique_ptr` and values, and all three legacy greps are at zero:
`EmptyAndDelete`, `SAFE_FREE` and `SAFE_DELETE` no longer exist anywhere,
definitions included.

**Raw ownership is not extinct, and stage 5 ending does not claim it is.**
**Three** things outlived the plan's scope, and none has an owning task. Two
more did and are **gone**: `EclButton::m_caption` and `m_tooltip` were
`new char[]`/`delete[]` with a copy each, and `strings-modernised/T11` retired
them on 2026-08-05 along with the widget `char[N]` members — `~EclButton` no
longer exists, because the class no longer owns anything. Then
`GlobalEventCondition::m_stringId` and `m_cutScene` went the same way in
`strings-modernised/T9`, which also took `NewStr` — `strcpy(new char[...])` —
out of the tree along with its other seven callers. The three without an owner:

```
SAFE_DELETE_ARRAY   2 callers in NeuronClient/Shape.cpp. The last of the
                    macro family; T7's acceptance named only the other two.
ColourShapeFragment new RGBAColour[1] into a ShapeFragment — Shape's
                    ownership, in NeuronClient.
Resource::ListResources
                    returns std::vector<char*>* — an owning vector of owning
                    char*, freed by every caller by hand.
```

**T6 was the last hard one, and two things it found are worth carrying:**

- **`~App()` NEVER RUNS.** `Species/Main.cpp` calls `Finalise()` and then
  `exit(0)`, which does not unwind the stack, and nothing anywhere deletes
  `g_app`. All seventeen `SAFE_DELETE`s were unreached code, and this file
  used to say the opposite — that subsystems reach each other during teardown
  and destruction order is load-bearing. **There is no teardown.** The order
  was preserved anyway, because it costs two lines and becomes true the day
  someone deletes `g_app`. But if you are reasoning about App's lifetime,
  start from the fact that it has no end.
- **The real risk was ownership that was already shared.** `g_renderer` and
  `g_taskManagerInterface` are deleted and rebuilt at RUNTIME from GameLogic
  and `Main.cpp` — on a resolution change and, until 2026-08-06, on a gamepad
  switch. **Only the resolution change is left**: the gamepad switch was
  `SwitchTaskManagerForX360Controller`, and `input-native-events` T2 deleted
  it, because no driver in the tree can report the mode it waited for, so it
  never actually executed. Taking `unique_ptr` ownership without touching those would
  have left App holding a freed pointer. Replacement now routes through
  `AppCommands`, whose factories install what they build. **Before converting
  a member, grep for who else deletes or reassigns it, not just who reads
  it.**

**Converting ownership turns silent hazards into build errors, and that is the
point.** T6's one CI failure was `can't delete an incomplete type` on
`AttractMode` — a type with no header anywhere in the tree, guarded by an
`ATTRACTMODE_ENABLED` that is defined nowhere. A raw pointer member had hidden
that for years; a `unique_ptr` member could not.

Two more rules that came out of doing the rest of it, both learned the
expensive way:

- **A task's file list is written from where ownership LIVES; the work is
  wherever the member is NAMED.** `ownership` T5 declared eight files and
  touched twenty-seven. Twelve of those hold no ownership at all — they only
  observe a converted member. Grep the member name tree-wide, with no
  assumption about the expression that reaches it, and read every hit.
- **Converting a raw pointer to `unique_ptr` can introduce use-after-move.**
  `x->field = v;` after `push_back(x)` is harmless with a raw pointer and
  undefined with a moved-from `unique_ptr`. Two were introduced and caught in
  T5. Scan every changed file for a local used after `std::move` of itself.

---

## Building

```powershell
msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64 /m
```

Visual Studio 2026 (toolset v145), C++23, Windows-only. ARM64 is the primary
development platform; x64 is also supported, and x64 Debug is the one
configuration CI builds. Full detail — configurations, what CI does not cover,
troubleshooting — is in [`docs/BUILD.md`](docs/BUILD.md).

---

## Before you push

Run all eight. CI runs the same eight and will fail on anything you skip.

```bash
python3 tools/check_project_files.py   # .vcxproj matches the files on disk
python3 tools/check_layering.py        # no new upward includes
python3 tools/check_task_dag.py        # task plans are valid DAGs
python3 tools/check_containers.py      # no legacy container call left on a vector
python3 tools/check_math_types.py      # no legacy math call left on a native type
python3 tools/check_sound_effects.py   # Effects.txt and the FX enum still line up
python3 tools/check_format.py          # changed lines match .clang-format
python3 tools/check_hygiene.py         # changed lines do not reintroduce NULL,
                                       # _included guards, strcpy or plain enum
```

`NeuronCore/MathUtils.h` no longer defines `min` and `max` macros, so
`std::min` and `std::max` compile everywhere now — they did not, anywhere that
header was reachable, until `language-hygiene` T8. If you find a hand-written
comparison with a comment apologising for it, that is why, and it can go.

`check_math_types.py` exists for the same reason and was written after five CI
failures in a row during `directxmath-migration` T10, every one a call site a
type sweep did not reach. Vector3 has methods and operators and XMFLOAT3 has
neither, so `vel.Mag()` stops compiling without ever mentioning the type's
name. It also reports two failure modes neither the compiler nor CI can see:
Vector3's default constructor zeroed and XMFLOAT3's does not, so a converted
member that something accumulates into changes behaviour silently; and a rename
that leaves a use behind, which binds to a different local rather than failing.

It has grown with every task since. It now resolves members, locals,
**function parameters**, `XMVECTOR`s, matrix rows and typed receivers — a
converted *signature* leaves its parameter's uses behind exactly as a converted
local does, and `obj->m_pos.Set(...)` names no type either.

Like `check_containers.py` it resolves by NAME, and **skips a name that means
two things rather than guessing** — sixteen are contended today, including
`m_pos`, `m_front` and `m_up`. That skip is the tool's governing trade:
under-reporting is recoverable, and crying wolf gets a check switched off. Every
gap ever found in it has closed by making a *narrower* claim about where a
name's type is known, never by broadening a regex; two rules that broadened
instead were measured, found to accuse correct lines, and refused. Keep that
shape if you extend it.

`check_containers.py` exists because three CI failures in a row were the same
mistake: a call site a container sweep did not reach, still asking a
`std::vector` for `Size()` or `ValidIndex()`. Those are compile errors, so CI
caught every one — after a full Windows build, which is the slowest possible
way to learn it. The check builds a tree-wide member-name-to-type map, which is
why it is a separate tool rather than a `check_hygiene` rule. It resolves by
member NAME, so a name that is a vector in one class and a slot map in another
is skipped and counted rather than guessed at; `m_buildings`, `m_spirits` and
`m_lights` are the three it currently loses.

`python3 tools/check_format.py --fix` applies the formatting rather than
reporting it.

The last two share a contract worth understanding before one surprises you: they
judge **the lines your change writes**, not the file you wrote them in. A legacy
file with two hundred `sprintf`s stays legal until its conversion task; add one
more and only that line is reported. It is a ratchet, so it only ever turns one
way. A genuine exception is marked `hygiene-ok` in a comment on the line, with a
reason — and **there are none left in the tree**. The last one was `Camera::Mode`
in `NeuronClient/CameraAccess.h`, and it went with the enum when
`language-hygiene` T12 scoped it on 2026-08-05. The mechanism is explained in
`tasks/Archive/language-hygiene.yaml` T1 and is still available; nothing is currently
using it, which is the state to keep it in.

Then build **and run the tests**. A change that has not been compiled is not
finished; a change with new behaviour and no test is not finished either.

```powershell
msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64 /m
vstest.console.exe ARM64\Debug\*Tests.dll /Platform:ARM64
```

CI builds and tests **x64 Debug only** — if you touched anything
ARM64-sensitive or optimisation-sensitive, build that configuration yourself,
because nothing else will.

**The project-file check matters more than it looks.** Adding a `.cpp` without
adding it to the `.vcxproj` produces no error — the file is simply never
compiled, and the symbols go missing at link time with no indication why. For a
test file it is worse: the suite still reports green, for a test nobody is
running.

---

## What working looks like

There are two separate questions here, and only one of them has an answer today.

**Does the code you changed still do what it should?** That is what the test
suite is for. `Tests/` holds one `<Name>Tests` project per library on the
Microsoft Native Unit Test Framework; CI builds and runs all of it on every
push. Write the tests with the change — [`docs/TESTING.md`](docs/TESTING.md) is
the standard, and it is not optional reading for anything that touches wire
format, the simulation, or a file being converted.

**Does the game work?** The suite cannot tell you. It covers a few hundred
lines out of 113,000 — encoding, string helpers, object identity — so "it
compiles and the suite passes" is not the same claim, and a green suite must
never stand in for the smoke test below.

The game **does** run, so the smoke test is something you can actually perform
rather than something to wait for. If you are working somewhere that cannot
launch a Windows client, say so instead of implying you checked.

**WHAT HAS AND HAS NOT BEEN COMPILED, as of 2026-08-06.** CI is green at
`ffc5105`, which carries all of `sound-xaudio2` and `input-native-events` T1-T2:
it compiles, links, passes the suite on x64 Debug, and the headless server
reaches sequence 20 at the right rate. Every task since `d858b6b` has gone green
first round.

**THERE IS AN UNCOMPILED BLOCK AGAIN, AND IT IS THE WHOLE OF
`input-native-events` T3 AND T5–T11.** Nine tasks are pushed behind `ffc5105`
and awaiting their own CI answer: the event stream, `WM_CHAR` text input, Raw
Input, the router and its consuming sinks, the subscription API, the collapse of
the event-handler layers, and the closing sweep. **Treat the green above as
covering `ffc5105` and not the branch head.** All nine were written on Linux
against the eight Python checks, a `g++`/ASan/UBSan harness for the parts that
are pure logic, and reading — no MSVC build and no client launch happened at
any point.

**What that combination can and cannot catch is the standing lesson of this
repository, and it applied twice in this block.** A Python check cannot see a
type that no longer matches its use; a sanitizer harness type-checks no call
site. What it did catch, both times by asking what a deleted `#include` had been
*supplying* rather than what it declared: `InputEvents.h` was getting `size_t`
through the `<vector>` that T6 removed, and `InputDriverWin32.h` was getting
`<windows.h>` through the `Win32EventProc.h` that T10 deleted while declaring a
`WndProc` in terms of `HWND` and `LRESULT`. Both would have been a CI round
each. **The earlier uncompiled block this entry used to warn about is gone**:
`strings-modernised` T12, T11, T13, T17 and T9 were the largest ever recorded
here, and CI has since built all of them.

**It took two red rounds to get there, and both were the same mistake.** An
earlier version of this entry claimed CI had been green on every task landed
here; it had not — `sound-xaudio2` T7 and T8 each failed to compile. Each
failure was a deleted `#include` taking declarations with it that the deleting
task never looked for. T7 removed `SoundLibrary3dDSound.h` from disk and left an
`#include` of it in `Species/Main.cpp`; the fix for that then removed the
include, and with it the only path by which that file saw `g_soundLibrary3d`,
which its `Finalise()` deletes. **Deleting a header withdraws everything it
included, not just what it declared.** Grep for the symbols the survivors still
use, not for the name of the type you removed.

**Compiled is still not run, with one exception.** The XAudio2 backend has been
heard once: the `sound-xaudio2` T5 audio gate below is an owner-reported run on
a 2026-08-06 build, and it exists precisely because CI cannot hear. **That gate
predates five tasks that changed what comes out of the speakers** — T6 made
XAudio2 the default, T7 and T8 deleted the other two backends, T4 rewrote every
DirectSound effect usage in `Sounds.txt`, and T9 made the music voice stereo. So
the audio has been heard, but not this audio. `sound-xaudio2` T11 is the gate
that closes that, and it is a full seven-step Garden run rather than a listen.

Nothing else from 2026-08-06 has been in front of a running game — not the input
changes, and no seven-step Garden run on any of it — so the baseline below is
unchanged at `acf283b`.

**`input-native-events` T12 IS THE GATE THAT CLOSES THIS, and it is worth
knowing what to look at rather than only that a run is owed.** Twelve of the
plan's thirteen nodes are code and all of them have landed; the run is the
thirteenth. Four things changed that no test in this tree can have an opinion
about, listed in the order they are most likely to be noticed:

- **Camera and cursor FEEL.** Aim comes from Raw Input now, which carries no
  Windows pointer acceleration — and because `TargetCursor` derives the visible
  cursor from the same control, the menu cursor changed with it. The intended
  gain is the other half of that trade: rotation continues while the pointer is
  pinned at a screen edge, which the old path could not do at all.
- **Typing.** Text comes from `WM_CHAR` rather than from a virtual key code and
  a shift flag, so a profile name should now be correct on any keyboard layout —
  and a shifted character is the quickest thing to check.
- **Clicking a menu.** A click the UI handles is consumed and no longer reaches
  the game bindings. One deliberate behaviour change rides with it: an order
  given in the frames just before a menu opens is now sent to the server instead
  of being discarded.
- **Escape and enter in menus.** Two of `SpeciesWindow`'s controls are
  subscriptions rather than polls, and the handler for one of them destroys the
  window from inside itself.

The `strings-modernised` block is worth remembering for what it taught rather
than for its size, which is why the description stays here now. It was 127
rewritten format strings, nine members changed from `char*` to `std::string`,
and four signatures narrowed, all written on Linux against the seven Python
checks alone. **What a Python check cannot see is a type that no longer matches
its use**, and that was the whole failure mode. Three things were compiled and
run with g++ 13 to narrow it before it ever reached MSVC — every
printf-to-`std::format` spec pair, the old and new `FileWriter::printf` side by
side on the real call-site formats, and the two non-mechanical rewrites against
their originals — and all three agree byte for byte. None of that is MSVC, and
none of it type-checks a call site.

Three CI rounds were needed to get the namespaced tree green, and what each
caught is worth knowing because none of them was findable by any check in
`tools/`:

| Round | What it was |
|---|---|
| 1 | a macro parameter named `name` capturing the `.name` member it read; a member of a global class defined inside a namespace; an explicit `::Type` that no longer named anything; a forward declaration left outside its wrapper |
| 2 | a block-scope `extern` binding to the global namespace |
| 3 | green |

**This is the smoke test.**

**The Garden.** It is the only location `GameData/Game.txt` marks available at
start (`Id 2, Avail 1`), built from `MapGarden.txt` +
`MissionGardenLiberate.txt`.

Launch, start a new profile, enter The Garden, and check:

| # | Expected | What it proves |
|---|---|---|
| 1 | The executable starts and the main menu renders | `GameData` staging and resolution work |
| 2 | The location loads without an assert | Landscape, level file and building parsing |
| 3 | **50 Citizens** spawn on team 0 — two groups of 30 and 20 | Entity creation from `InstantUnits` |
| 4 | **179 Virii** spawn on team 1 across eight groups | Multi-team spawning |
| 5 | Both move under their own behaviour for 30s with no assert | `Location::Advance`, the slice loop, entity AI |
| 6 | The Task Manager opens and lists available programs | Eclipse UI and `GlobalResearch` |
| 7 | `GenerateSyncValue()` does not trip the sync assert in single player | The simulation is at least self-consistent |

Those counts are read from `MissionGardenLiberate.txt`, so they are checkable
rather than approximate. Any step failing localises the break to a subsystem.

**TWO AUDIO RUNS, 2026-08-06, both owner-reported.** Nothing in CI can hear,
so these are the only evidence that exists for the whole `sound-xaudio2` plan.

The first was the T5 A/B gate, on the branch with `SoundLibrary = xaudio2`
while DirectSound was still present and still the default for everyone else:
*"the audio works fine"*. That is what unblocked deleting the DirectSound
backend, the software mixer and the waveOut layer.

The second was the T11 final gate, on a **default** build — no preference
overrides, because there is no longer a preference to override with, and no
legacy backend left in the binary to fall back to: *"the audio smoke test is
done and everything works fine"*. It closes the plan at 11 of 11.

**Both are verdicts on a run, not per-step breakdowns**, and this file does not
claim breakdowns it was not given. Three things the second gate specifically
did NOT establish, listed because a pass is easy to over-read:

- The audio checklist was not itemised. Positioned effects, the six rewritten
  echo events, the two converted reverbs, stereo music and the volume slider
  were all in the build and none was reported wrong — which is weaker than each
  having been confirmed.
- **The device-loss path was never exercised.** Nobody unplugged anything. T10's
  `OnCriticalError` teardown and the five-second rebuild in
  `SoundSystem::Advance` have been traced under ASan and UBSan and compiled by
  CI, and that is the whole of it. It is the least-evidenced code in the plan.
- Distance attenuation is still the open question from the first gate.
  X3DAudio's curve is not bit-identical to DS3D's and the two were never
  compared directly, so sounds fading at the wrong range is what neither run
  was equipped to catch.

**Neither replaces the baseline below.** No seven-step Garden run with an
explicit per-step breakdown has been performed on any 2026-08-06 build.

**CURRENT BASELINE: `acf283b` (2026-08-05), owner-reported, ALL SEVEN STEPS
PASS.** This is the most recent full run, the most recent run with an explicit
per-step breakdown, and the build any future divergence is measured against.

It closes `determinism` T6 and with it that whole plan, 6 of 6 —
`tasks/Archive/determinism.yaml`. It was run on the Batch 5 branch rather than
on `main`, which is more than the gate asked for: **it is also the first
running-game evidence for Batch 5's four tasks** — `ownership` T11,
`language-hygiene` T10 and T13, `strings` T18 — which until then had only ever
been checked by CI. Steps 2, 5 and 6 are the ones that would have caught a
mistake in them.

**THE SIMULATION MOVED, AND THIS IS WHERE IT MOVED TO.** `determinism` T5
postdates the `1af4979` baseline below and changes the `syncrand` call
sequence, so the sync value this build produces is NOT the one `1af4979`
produced. A client from before T5 desyncs against one after it. That cost was
accepted on T1 and T5 and is now confirmed against a running game rather than
assumed.

What the run does **not** cover: three of T5's six fixed sites — Spam, GodDish
and Library — are not in The Garden and remain unexercised. Laser fences and
incubators are, and `LaserFence.cpp:66` was the one outright desync of the six.
It also does **not** discharge `ownership` T6. That task has since landed and is
done, but its own acceptance asked for the main menu after each of its four
commits and nobody performed that; see *How work is broken down* below.

**Previous baseline: `1af4979` (2026-08-05), owner-reported successful,
on the WRAPPER-FREE build.** This is the run `directxmath-migration` T27 was
waiting on, and it closes that plan: `Vector2`, `Vector3`, `Matrix33` and
`Matrix34` are deleted, there is no conversion seam, and every math value in
the tree is a DirectXMath type. `bb4a110` sits on top of it and is
documentation only, so the binary is `1af4979`'s tree.

**This was the baseline until `acf283b` above.** It is the run that established
that the sync value is whatever native math produces; a build from before the
migration does not agree with it, and is not supposed to. It stopped being the
measuring stick when `determinism` T5 shifted the RNG sequence on top of it.

Why the run was needed when CI was green: T25 deleted the seam, which changed
how every converted call site resolves its types, and three of this migration's
worst defects were invisible to both the compiler and CI — a member that
stopped zeroing itself, a matrix column left as stack garbage, and a normalise
of an exactly-zero cross product. All three were found by the owner looking at
the game. None of them would fail a build.

**Last full run before that: all seven steps pass at `b0bde71` (2026-08-03), on the
renamed build — the whole of stage 3, the `Darwinian`→`Citizen` rename and
six of the stage-4 string conversions.** Reported by the project owner, not
observed by the agent that wrote this line. This is the run
`rename-darwinian/T4` was waiting for, and it closes that plan: the spawn
counts are what catch a string-resolved reference the rename missed, because
a name that fails to resolve produces a smaller group rather than a crash.

**Run at `36dd038` (2026-08-04), on the DirectXMath migration's converted
engine layers** — NeuronCore's math and geometry, NeuronClient's renderers and
sound, and the wire types. Owner-reported: the game runs. One observation was
open at the time — the procedurally generated landscape looked different in
shape, shading unaffected — and it is now **closed**: the height-map checksums
are equal on both builds, so the terrain is bit-identical and the difference
was in rendering or in the eye. The temporary checksum commit that answered it
(`57386fb`) has been reverted. `tasks/Archive/directxmath-migration.yaml` T13 carries
the detail.

**Run at `bd03d4e` (2026-08-05), owner-reported: THE SMOKE TEST FINISHED
SUCCESSFULLY.** This is the run `directxmath-migration` T21 and
`determinism` T2 were both waiting on, and one run closed both — they are the
same seven steps on the same build, and Spirit.cpp carried both an RNG change
and a math conversion into it. It is the first time the GameLogic wave
(T14–T19: entities, creatures, buildings, world, effects, weapons) has been in
front of a running game, and the first Garden run since `b0bde71`.

Reported as "smoke test finished successfully", with trees confirmed visible
again. That is the owner's word for the whole run; this file does not claim a
per-step breakdown it was not given.

**Two objects were reported missing during that run. One was a real defect and
one was not, and the difference is worth keeping.**

- **A research item was not there — and that was correct.** The owner had
  raised their research level, and `ResearchItem::Advance` removes an item
  whose research you already hold at that level. Working as designed. Recorded
  so nobody investigates it a second time: *a missing research item is the
  expected result of already owning that research.*

- **THE TREES HAD DISAPPEARED, and that was real.** Found and fixed — `Tree::RenderBranch` crossed two vectors that
are identical on the trunk, so the cross product was exactly zero, and
`XMVector3Normalize` answers zero where `Vector3::Normalise` answered (0,0,1).
Zero there collapses the trunk quads to zero width *and* hands all four child
branches a zero right-angle, so the whole tree became an invisible vertical
line. `directxmath-migration` T17 converted the file; the audit that was
supposed to catch this missed the one site that degenerates **unconditionally**
rather than in an edge case. The fallback is now reproduced locally in
`Tree.cpp` with the reason at the site, and `NeuronMathTests` pins the
divergence with a negative control.

Confirmed fixed by the same run: the trees are visible again.

> This is what the smoke test is *for*, and it is worth stating plainly: seven
> green checks, two green CI runs and 180 passing tests all said this tree was
> fine. None of them renders a tree. The owner looking at the game found it in
> one run — and in the same run reported a second missing object that turned
> out to be the game behaving correctly. Both halves of that are the point: the
> smoke test finds what CI cannot, and a report from it still has to be
> diagnosed rather than believed.

Earlier runs, kept because the sequence is the evidence:

- **All seven steps, on the layering-inversion branch (2026-08-02)**, after
  the `g_app` seam moved the world subsystems, the frame clock and App's state
  out of the executable.
- **Partial run at `586c072` (2026-08-03)**, after containers-replaced T12
  converted the world's slot containers off `DArray` and the entity rename
  landed — steps 1, 2, 5, 6 and 7 only. Steps 3 and 4 went unchecked, which is
  why it did not close `rename-darwinian/T4` and the run at `b0bde71` did.

Those runs are the reason the changes under them were merged: CI proved they
compile and the unit suite passes, and neither says anything about whether the
game still starts, spawns and advances.

**Record what you find here.** The value is in it being current, not
aspirational. If a step starts failing, say which one: that is the difference
between "the game is broken" and a named subsystem to look at.

---

## How work is broken down

Anything larger than a single-file change is expressed as a **directed acyclic
graph** of tasks in a YAML file under `tasks/` before code is written. Nodes are
tasks, edges are dependencies, and the graph is validated in CI.

```bash
python3 tools/check_task_dag.py --next  tasks/<plan>.yaml   # what can I start?
python3 tools/check_task_dag.py --waves tasks/<plan>.yaml   # what can run in parallel?
```

Set a task to `in_progress` and commit that *before* starting it, so a concurrent
agent does not pick up the same node. Set it to `done` only when every `verify`
command passes, and commit the plan update with the code.

`--next` also reports tasks it will *not* offer yet, under "held by another
plan". Those carry a `blocked_by` edge into a different plan file — the
modernisation stages run per file across three separate plans, so "this file
finishes stage 3 before it starts stage 5" is an ordering no single plan's graph
can see. Trust it: before those edges existed, `--next` offered every
`tasks/Archive/ownership.yaml` task on files whose stage-3 conversion had not begun.

The full standard — schema, status semantics, how to write acceptance criteria,
how concurrency works — is [`docs/TASK_DAG.md`](docs/TASK_DAG.md). Read it before
writing your first plan.

**If you are asking "what should I do next", ask the tool** —
`python3 tools/check_task_dag.py --next tasks/<plan>.yaml` across the three
open plans listed under *Current priority*. This paragraph used to say there
was no answer because every plan was finished; that stopped being true on
2026-08-06. Work outside those three still needs its own plan written before
code is written.

[`tasks/_next-batch.md`](tasks/_next-batch.md) is kept as the record of how the
seven batches were scheduled: what was ready, measured; which ready tasks
collided over which files, which `--next` could not tell you because it reasons
one plan at a time; and, each time, how the prediction turned out. **Read it
before writing a plan, not to find work.** Its recurring finding is the
transferable part: a task's declared `files` list is written from where a thing
is DECLARED and the work is wherever it is NAMED, and eight of the nine lists
measured against the tree were wrong.

**ELEVEN PLANS ARE COMPLETE AND IN `tasks/Archive/`. THREE MORE ARE OPEN
BESIDE THEM** — see *Current priority* for what they are and where they stand.
`determinism`, `ownership`, `language-hygiene` and `namespace-migration` all
closed on 2026-08-05, and `strings-modernised` — the last one — closed the same
day at 20 of 20. T13 narrowed the read-only name parameters to `string_view`,
T17 replaced `FileWriter::printf` with `std::format` templates, and T9 swept
the `strcpy`/`sprintf` family to a tree-wide zero and deleted `NewStr` with it.

**Ready tasks exist**, and `--next` will name them — but as of 2026-08-06 they
are all in ONE plan. `input-native-events` has no ready node left: twelve of its
thirteen are done and the thirteenth is an owner Garden run, so `--next` reports
nothing for it. Everything schedulable is in `network-transport`, which has not
started. Work outside the open plans still needs a plan written for it before
code is written — see *How work is broken down* below and
[`docs/TASK_DAG.md`](docs/TASK_DAG.md). What the closed plans left behind, each
recorded and still unowned: the three raw-ownership survivors above, the unswept
LCG sites below, entity and building behaviour having no tests, `GlobalWorld`
not being constructible in a test DLL, and the four `input-native-events`
leftovers under *Known issues*.

The tree is namespaced; that is its own section, [above](#namespaces).

**Migration stage 5 is finished**, and the scheduling problem the batch files
exist to solve went with the last of those plans. It came back on 2026-08-06 and
has largely gone again: of the three plans opened that day, `sound-xaudio2`
closed at 11 of 11, `input-native-events` is at 12 of 13 with only its owner gate
left, and `network-transport` has 12 tasks none of which has started. So there is
no collision to schedule around today — one plan is open for work and nothing
else is touching those files. `check_task_dag.py --next` answers what to
schedule rather than any of the batch files, which describe the eleven that
closed. **Two nodes are gated on the owner rather than on code**, and both are
Garden runs: `sound-xaudio2` T11 is discharged, `input-native-events` T12 is
not.

**`ownership` T6 is still the one to be sceptical about.** Its acceptance asked
for the game to reach the main menu after each of its four commits, and that
check was NOT performed; CI compiled them and nothing more. See its notes for
what a smoke test can and cannot say about it — the destructor it rewrote is
unreachable code, and of the two runtime paths it did change, one needs a
resolution change to reach and is not a Garden step, while the other, the
gamepad switch, turned out to be unreachable in any case and was deleted by
`input-native-events` T2 on 2026-08-06. So T6 is now MORE doubtful, not less:
half of what it changed can never have run.

The two older reading orders are still there and still worth reading, but
neither answers "what next" any more:

- [`tasks/_restart.md`](tasks/_restart.md) is the modernisation restart of
  2026-08-03. Its ordering has been executed and its counts are historical; what
  survives is *why* the plans are shaped as they are, and the recurring failure
  mode it names — a task list written from grep counts rather than from reading
  call sites.
- [`tasks/_restart-directxmath.md`](tasks/_restart-directxmath.md) is the math
  migration's handover. **That plan is complete and archived**; the file is kept
  for the five ways that conversion broke files nobody touched, which is the
  transferable part.

---

## Rules

**Match the surrounding code.** Two styles coexist here. Which one applies
depends on the file you are in, not on your preference. See
[`CODING_STANDARDS.md`](CODING_STANDARDS.md).

**Keep modernisation in its own commit.** If you are fixing a bug in a legacy
file, fix the bug. Converting the file is valuable but it is a separate task with
its own plan entry — mixing the two produces a diff nobody can review.

**Never reintroduce the layering allowlist.** It went from 628 entries to
zero and was deleted. An upward include is now a build-stopping error with no
way to record an exception, which is the point — `tasks/Archive/layering-inversion.yaml`
has eighteen worked examples of removing one properly. The same check also
catches a symbol declared in a library header and defined only in an executable,
which is the same reach with the linker doing the work instead of the
preprocessor — the tree carried one for years, `WindowManager.h` declaring
`AppMain()` for `Species` to define (T18). Class members are exempt, because a
pure-virtual declared low and overridden high is dependency inversion, which is
how most of the 628 were removed.

**Do not change what the simulation computes.** Multiplayer is deterministic
lockstep with a runtime checksum, so iteration order, floating-point arithmetic
order, container choice and the `speciesRandom()` call sequence are all
load-bearing. A refactor that looks purely cosmetic can desync the game while
every build stays green. `SlotMap` in particular has indices that are network
identity — it is not a `std::vector`, and `FastSlotMap` and `SlotMap` assign
different ones after a removal. Read
[`CODING_STANDARDS.md`](CODING_STANDARDS.md#determinism) before touching anything
reachable from `Location::Advance`.

**Do not reformat files you are not otherwise changing.** Formatting is enforced
on changed lines only, deliberately: a repo-wide reformat would destroy `git
blame` across 113,000 lines. Whole-file formatting is a migration task, done
deliberately, one file at a time, in its own commit.

**Test what you build.** New behaviour ships with the tests that cover it, in
the same change — not as a follow-up node. What earns a test, what does not, and
what a test is allowed to touch are in [`docs/TESTING.md`](docs/TESTING.md).
Anything on the wire or anything the simulation depends on being identical
everywhere earns one every time.

**Say when something did not work.** A compile is not a test, and a green suite
is not a running game. Report what you actually ran: "it builds and the suite
passes; I could not launch the client" is the honest sentence whenever you are
working somewhere that cannot run a Windows build, which is most agents most of
the time. Write that, rather than implying more. The game running again raises
what *can* be checked, not what you may claim without checking.

**Ask before changing build topology.** Adding a project, changing the toolset,
adding a dependency, or restructuring the solution affects everyone's build. It
warrants a question first.

---

## Known issues

Real, currently true, and worth knowing before you trip over them:

- **The game runs, and almost nothing here proves your change kept it that
  way.** The most recent owner-reported successful run is `acf283b`
  (2026-08-05), and it is also the most recent with an explicit
  all-seven-steps breakdown — see *What working looks like*. CI builds and runs
  the unit suite; it does not launch the client, and neither does any agent
  working on Linux. A change that compiles and passes 198 tests can still break
  the game on the first frame.
  - Batch 5 is the standing example in both directions. Four tasks went in on
    CI evidence alone; CI then caught two real compile errors a name-keyed
    sweep had missed, and the owner's run afterwards is the only thing that
    says the tree still plays. Neither check substitutes for the other.
- **RESOLVED BY DELETION, and recorded because the shape recurs:** both legacy
  sound backends read one element past the end of `SoundSystem::m_channels`
  once per audio tick. That array is sized from `GetNumMainChannels()`, which
  is `m_numChannels - 1`, but `SoundLibrary3dDSound` and
  `SoundLibrary3dSoftware` both pumped channels up to `m_numChannels` and
  handed the index straight to the main callback, which subscripts the array
  with it. Benign in practice — the garbage `SoundInstanceId` failed a
  bounds-checked slot map lookup and that channel wrote silence — but real UB
  in the path that shipped. It was deliberately NOT fixed when found, because
  `sound-xaudio2` T7 and T8 were going to delete both files and a fix would
  have needed justifying against a smoke test nobody would run on code that was
  going away. They are gone, and `SoundLibraryXAudio2` sizes its channel vector
  from `GetNumMainChannels()` so the callback can only ever be handed an index
  the array holds.
- **THERE ARE TWO RANDOM STREAMS, AND THE DOCUMENTATION SAID THERE WAS ONE.**
  This is the correction that retired the two determinism bullets that used to
  stand here, and it is the thing to know before reading any RNG call in this
  tree:

  | | Where | For |
  |---|---|---|
  | `syncrand` / `syncfrand` / `syncsfrand` | `NeuronCore/MathUtils.cpp`, Mersenne Twister | **the simulation** — 345 sites in `GameLogic` |
  | `speciesRandom` / `frand` / `sfrand` | `NeuronCore/Random.cpp`, LCG over `holdrand` | **cosmetics** — particles, render jitter, UI, sound |

  `syncrand` is the lockstep stream. `speciesRandom` is not, and cannot be made
  so — terrain and tree generation reseed it wholesale, and sound and the UI
  consume it at a client-dependent rate. `MathUtils.h:12` has said this since it
  was inherited; `CODING_STANDARDS.md#determinism` contradicted it until
  2026-08-05 by declaring `speciesRandom()` the only source, and
  `tasks/Archive/determinism.yaml` T3 and T4 are the reading that settled it.
  - **Two findings that used to sit here were false alarms, and both are
    closed.** `SoundInstance.cpp`'s two draws (now lines 547 and 1035) and
    `LandscapeRenderer::GetLandscapeColour`'s per-vertex reseed do vary in
    count between clients — that part was right — but they vary the LCG, which
    the simulation never reads. T3 and T4 carry the per-call-site conditions
    and the evidence. Do not reopen either without reading them.
  - **The real bug ran the other way and there were six of them**, all
    simulation state drawn from the client-local generator. `determinism.yaml`
    T5 fixed them: `LaserFence.cpp` (a spark timer from `frand` that then gated
    a `syncfrand` draw inside `Advance` — the one outright desync),
    `LevelFile.cpp` ×2 (centipede spawn positions, summed by
    `GenerateSyncValue`), `Incubator.cpp` ×2 (spirit positions), `Spam.cpp` and
    `ResearchItem.cpp` (building facing, which reaches `m_centrePos` and from
    there the position of every SpamInfection in `m_effects`).
  - **The RNG sequence changed with that fix**, so a client carrying it
    desyncs against one without — the same cost `determinism.yaml` T1 accepted
    and for the same reason. **T6 confirmed it against a running game on
    2026-08-05 at `acf283b`, all seven steps, and that closed the plan** —
    `tasks/Archive/determinism.yaml`. The shifted sequence is why `acf283b`
    rather than `1af4979` is now the simulation baseline.
    - **Three of the six sites are still unexercised.** `Spam.cpp`,
      `GodDish` and `Library` do not appear in The Garden, so that run says
      nothing about them; they will first be seen on a level that has them.
      Laser fences and incubators were in front of the owner.
  - **Nobody has swept the remaining LCG sites.** 186 of them were classified
    far enough to find the six; a site-by-site record of which feed simulation
    state does not exist and has no owning task.
- **`input-native-events` LEFT FOUR THINGS RECORDED AND UNOWNED**, swept up by
  its T11 and listed here because that plan closes without them. None has a
  task; each is small and each is a decision rather than a defect.
  - **A re-introduced controller has to bring its art back.** T2 deliberately
    KEPT the twenty `INPUT_MODE_GAMEPAD` consumers — the per-mode phrase table,
    the 360 button prompts, the whole `ControlHelp` overlay — arguing they are
    the controller *user interface* and that deleting them turns a future
    controller into a UI rewrite rather than a new driver. The owner then
    deleted the 22 gamepad button BMPs under `GameData/Icons` (`ca1a5c5`). Both
    halves are unreachable today, because no driver produces that mode and every
    load of that art sits behind a test for it — so **adding a controller driver
    is not sufficient on its own; the icons must be restored in the same work,
    or that UI code must go with them.** T11 chose to write this down rather
    than delete a coherent feature on its own authority; deleting it is a
    one-line decision for the owner.
  - **`controlIcon` has no consumer.** Nothing anywhere renders a control icon.
    The `~` icon lines in the prefs files are parsed into a map that is never
    read. Removing the plumbing is `ControlBindings` surgery and had no
    acceptance criterion behind it in that plan.
  - **`ControlMenuClose` has no binding** in any file under `GameData/Input`, so
    the escape-to-close-a-window handler cannot fire. That is data, not code —
    a one-line addition if the feature is wanted.
  - **The `Chord` and `Idle` drivers have no data exercising them.** T13 deleted
    the last `++` and `idle` lines in the tree. T11 decided they STAY, and the
    reasoning is worth knowing before anyone revisits it: unlike `InputDriverPipe`,
    which T1 deleted because it was never *registered* and so unreachable by any
    line a user could write, these two are registered and still work today for
    anyone who writes such a binding. Deleting them removes a capability of the
    rebinding language, not dead code.
- **Mixed-architecture play is NOT SUPPORTED.** Not "unproven" — decided. The
  simulation computes on DirectXMath, which dispatches to SSE on x64 and to
  ARM-NEON on ARM64, and the owner decided on 2026-08-03 to accept that rather
  than force the scalar path: no `_XM_NO_INTRINSICS_`, no
  `<FloatingPointModel>`, no build-topology change. Two lane implementations do
  not produce bit-identical results, deterministic lockstep requires that they
  do, and the 281 `sinf`/`cosf`/`powf` calls in simulation code were already a
  second reason before the first one existed. An ARM64 client and an x64 client
  in one session will desync.
  - **Within one architecture the simulation stays deterministic**, which is
    what the sync assert in `Server.cpp` tests, and that is the property the
    migration was required to preserve. Two x64 clients agree; two ARM64
    clients agree.
  - Making them agree with each other is a project in its own right — pinning
    the float model and auditing every transcendental — and nothing in the tree
    is waiting on it. Do not treat this bullet as a bug report.
- **ARM64 Debug is not gated by CI.** CI builds x64 Debug only, so the primary
  development platform is never checked here — build it yourself before relying
  on it. The unexplained ARM64 Debug failure that used to be recorded in this
  spot has not recurred and is now attributed to the C3859 memory pressure
  described below; it is written up there as a resolved instance rather than
  kept as a standing mystery.
  - Adding ARM64 to CI was proposed and **declined on 2026-08-02**: the arm64
    runner is a preview image that roughly doubles wall clock, and ARM64 is built
    constantly at the desk anyway. Deliberate, not an oversight.
- **Release is not built by anyone.** Three template leftovers — missing include
  paths, a precompiled header nothing created, and `Species` linking Release as a
  console app when `WinMain` is its entry point — are all fixed, and
  `Species.vcxproj` now sets `SubSystem` to `Windows` in both configurations. But
  the first two were confirmed by a CI run and the third landed after CI dropped
  to Debug-only, so **Release has still not been built since**. CI does not gate
  it and nobody builds it by hand, which means it can be broken right now and
  nothing would say so. Build it locally before anything that ships. Details in
  [`docs/BUILD.md`](docs/BUILD.md).
  - Adding a Release build to CI was proposed and **declined on 2026-08-02**:
    `ci.yml` argues Release differs from Debug in optimisation settings alone and
    catches little Debug does not, and that reasoning still holds. This bullet is
    the accepted cost of that, not an oversight — do not re-propose it without a
    Release-only break to point at.
- **The test suite is thin.** Four projects, **198** tests as of
  `strings-modernised` T9 (2026-08-05). The seven newest are the five in
  `BinaryReaderTests` — the filename extension parse, including the empty
  answer for a name with no dot and a 400-character name that the char[256]
  behind it could not have held — and the two `FileWriterTests` gained for
  behaviour T17 created: a percent sign in argument-free text is now a percent
  sign, and text longer than the old 10240-byte stack buffer is written whole.
  Before them, `ControlBindingsTests` characterise the control-name lookup —
  including the edge where the EMPTY STRING matches and returns a real,
  bindable `ControlNull`. Before them the count was 184, one FEWER than the run
  before that, which is rare here and worth spelling out: `ownership` T7 deleted
  the transitional `SlotMap::EmptyAndDelete` helper and the single test
  characterising it went too. The rest
  cover IP conversion,
  the `speciesRandom` sequence, the `ByteStream` macros, both halves of the wire
  format (`NetworkUpdate` and `ServerToClientLetter`), the `FilesysUtils` path
  helpers, `WorldObjectId` including its 16-byte wire layout, the state a new
  `Server` starts in, the legacy containers plus their `Neuron::SlotMap`
  replacement, the preferences file format, the bytes `FileWriter::printf`
  emits for every format the level and profile writers use, `LevelFile`'s
  constructors, the native-math conversions and
  geometry routines, the entity grid, the routing system's waypoints, the slice
  walker, `InputField`'s keystroke write-back, `ShapeMarker`'s parse of a marker
  block, and the two `Matrix33` rotation
  mappings — each with a negative control asserting that the intuitive reading
  is measurably wrong. That is the encoding, identity
  and protocol layer plus a thin skin over the rest — no entity behaviour, no
  rendering, no level loading, and nothing at all that would notice the game
  failing to start.
  - **This figure has been wrong here before, twice.** It said 180 when CI
    counted **169** at `e7a1a88`, and `strings-modernised` T8's eleven then made
    the stale number true by accident; it stayed at 180 while `strings/T20`
    added five more. Read the count off a CI run's *Total
    tests* line or off `git grep -c TEST_METHOD -- 'Tests/*.cpp'`, and note that
    those two agree only because every `TEST_METHOD` in the tree is compiled —
    a test file missing from its `.vcxproj` would make the grep the higher of
    the two, which is the shape of the failure `check_project_files.py` exists
    to prevent.
  - The preferences tests are worth the paragraph they cost, as an argument for
    writing more of them. They were added as characterisation before a
    conversion (`containers-replaced` T19) and the first CI run was red: three
    of the four failures were **access violations in shipped code**, not test
    bugs. `SetInt` on a key an existing preferences file did not contain
    crashed on shutdown; column-aligning that file with more than one space
    before `=` crashed the next save. Both are ordinary things for a user to
    do, both had been there since the file was inherited, and neither was
    findable by reading. 21 tests over one 575-line file found them in an
    afternoon.
  **`Tests/GameLogicTests/LinkStubs.cpp` is now empty**: `GameLogic` no longer
  names a symbol the executable owns, so it links into a test DLL on its own.
  Entity and building behaviour is finally testable, and still largely untested.
  `layering-inversion` T11 is **done** — it wrote ten tests — but they cover
  spatial indexing (`EntityGridTests`) and waypoint ordering
  (`RoutingSystemTests`), which is not entity or building BEHAVIOUR. That gap
  has no owning task; do not follow T11 expecting to find one. `Species` and `Server` have no
  test project at all — an `.exe` cannot be linked into a test DLL, so code in
  either that is worth testing belongs in a library. See
  [`docs/TESTING.md`](docs/TESTING.md).
- **C3859 / C1076 is memory pressure, not a code fault.** "Failed to create
  virtual memory for PCH" / "compiler limit: internal heap limit reached",
  landing on a different file every run and on whichever project happens to be
  compiling. These PCHs are large and each `cl.exe` has to map one; on a 16 GB
  machine with Visual Studio open (`devenv` alone holds ~1.7 GB, plus the
  ReSharper backend) there is not always room. Observed with ~1.8 GB free.
  - It is **not** the 32-bit-host case [`docs/BUILD.md`](docs/BUILD.md)
    describes. It reproduces on an ARM64 host building x64, forcing
    `PreferredToolArchitecture` changes nothing, and it happens with no test
    projects in the solution — so it is neither new nor caused by them.
  - `/m:2` and `/m:1` do not reliably avoid it; a single `cl.exe` short of
    address space is enough.
  - **Close Visual Studio, or re-run the build.** Each pass gets further, and a
    project that failed in a solution build usually succeeds when built alone.
  - Lingering MSBuild nodes make it worse — node reuse leaves a dozen alive.
    `/nr:false` if they accumulate.
  - It is the accepted explanation for a one-off ARM64 Debug failure that used
    to be recorded above as unexplained: it failed after `NeuronClient.lib`
    linked, the error scrolled past the retrievable log tail, and a later run got
    much further without failing. Same symptom, same "passes on a re-run"
    behaviour. It has not recurred. **If an ARM64 Debug failure turns up that is
    not this, capture the first `error C...` line and record it here.**
- **Provenance is unresolved.** [`LICENSE`](LICENSE) states the project's terms —
  internal research, non-commercial, not for distribution — but those terms cover
  only this project's own contributions. The licence covering the original
  Darwinia source has never been established, so nothing here may be published,
  redistributed or used commercially without settling that first. **One** file
  carries a third-party notice that must never be stripped, including when moved
  or modernised — `NeuronCore/MathUtils.cpp`, under BSD 3-clause. This bullet
  said three; `LICENSE` has listed one since `AutoVector.h` and `TriTri.cpp`
  were deleted, notices included. `LICENSE` used to carry two paragraphs
  explaining why each of those rows went; the owner removed them on 2026-08-05,
  so that table now describes what the repository contains and nothing more.
  The reasoning survives in git history and in `containers-replaced` T16 and
  `directxmath-migration` T4. Neither file is in the tree, and nothing of
  either author's is shipped stripped of its terms.
