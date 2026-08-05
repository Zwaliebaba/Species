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
across six MSBuild projects. It links only against the OS (OpenGL, GLU, WinMM,
DirectSound, Winsock) and takes one header-only dependency, **DirectXMath**,
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

**Cleanup and modernisation first.**

The near-term goal is to finish converting the inherited Darwinia code into
Neuron-style C++23 with enforced layer boundaries. A runnable game and the
authoritative world server are later milestones that depend on this landing.

What that means for a task in front of you:

- **In scope:** modernising legacy code, removing upward layer dependencies,
  replacing hand-rolled containers with standard ones, killing raw owning
  pointers, tightening project structure, improving the build and the checks.
- **In scope but secondary:** fixing things that are outright broken, when you
  encounter them in code you are already changing.
- **Out of scope:** new gameplay features, world/persistence systems, netcode
  redesign, renderer rewrites. Not because they are unwelcome — because the
  foundation is not ready and the work would have to be redone.

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
> recently at `1af4979` (2026-08-05). That is recorded here because
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

NeuronCore/       ~2.4k   Foundation: sockets, threads, byte streams, the wire
                          protocol, filesystem, assertions, and the math
                          conventions in NeuronMath.h. It holds no math TYPES:
                          Vector2/3, Matrix33/34 and Plane were deleted, and
                          storage is DirectXMath's own. Static library.
NeuronClient/     ~30k    Presentation: OpenGL renderer, sound, input drivers,
                          the Eclipse UI toolkit, resource loading. Static library.
NeuronServer/     ~0.5k   Authoritative simulation host: Server, ServerToClient,
                          the client and team registries. Static library.
GameLogic/        ~48k    Entities, buildings, teams, unit behaviour, in-game
                          windows. The bulk of the inherited code. Static library.
Species/          ~32k    Client executable: app, world, camera, landscape,
                          task manager, level loading.
Server/           ~0.1k   Headless server executable. Links NeuronCore and
                          NeuronServer only; ticks the host at 10 Hz.

Tests/            ~0.4k   One <Name>Tests project per library, on the Microsoft
                          Native Unit Test Framework. Built and run by CI.
tools/                    The checks CI runs. Run them locally too.
tasks/                    Task DAGs. See docs/TASK_DAG.md. Start at
                          _next-batch.md — what is ready, what collides,
                          and what the current batch is. Finished plans
                          live in tasks/Archive/.
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
NeuronCore                   no dependencies
  NeuronClient               -> NeuronCore
  NeuronServer               -> NeuronCore
    GameLogic                -> NeuronCore, NeuronClient, NeuronServer
      Species  (exe)         -> GameLogic, NeuronClient, NeuronCore
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

## Ownership

Migration stage 5 — raw owning pointers become `std::unique_ptr` and values —
is nearly done, and what is left is worth knowing because two greps that used
to be noisy are now almost silent.

```
EmptyAndDelete   ZERO call sites. Only the two transitional definitions
                 remain, in NeuronCore/SlotMap.h and NeuronCore/VectorUtils.h,
                 and both are now dead code.
SAFE_FREE        ZERO call sites. Gone with ShapeMarker's names.
SAFE_DELETE      17, all in Species/App.cpp, plus the definition in
                 NeuronCore.h. That is ownership T6 and then T7.
```

**`App.cpp` is the whole of what is left of the macro**, and it is deliberately
last: it constructs and tears down the entire subsystem graph, and subsystems
reach each other during teardown, so construction and destruction ORDER is
load-bearing. `ownership` T6 does it in small commits, each one owner-verified
to reach the main menu. T7 then deletes the macros, and the two transitional
`EmptyAndDelete` helpers can go with them — they have no users left.

Two rules that came out of doing the rest of it, both learned the expensive
way:

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

Run all seven. CI runs the same seven and will fail on anything you skip.

```bash
python3 tools/check_project_files.py   # .vcxproj matches the files on disk
python3 tools/check_layering.py        # no new upward includes
python3 tools/check_task_dag.py        # task plans are valid DAGs
python3 tools/check_containers.py      # no legacy container call left on a vector
python3 tools/check_math_types.py      # no legacy math call left on a native type
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
`tasks/language-hygiene.yaml` T1 and is still available; nothing is currently
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

**POST-MIGRATION BASELINE: `1af4979` (2026-08-05), owner-reported successful,
on the WRAPPER-FREE build.** This is the run `directxmath-migration` T27 was
waiting on, and it closes that plan: `Vector2`, `Vector3`, `Matrix33` and
`Matrix34` are deleted, there is no conversion seam, and every math value in
the tree is a DirectXMath type. `bb4a110` sits on top of it and is
documentation only, so the binary is `1af4979`'s tree.

**This run is the new baseline for the simulation.** From here the sync value
is whatever native math produces, and any future divergence is measured
against this build rather than against anything before it. A build from before
the migration does not agree with this one, and is not supposed to.

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
`tasks/ownership.yaml` task on files whose stage-3 conversion had not begun.

The full standard — schema, status semantics, how to write acceptance criteria,
how concurrency works — is [`docs/TASK_DAG.md`](docs/TASK_DAG.md). Read it before
writing your first plan.

**If you are asking "what should I do next", start at
[`tasks/_next-batch.md`](tasks/_next-batch.md).** It is the cross-plan
scheduling argument: what is ready, measured; which ready tasks collide over
which files, which `--next` cannot tell you because it reasons one plan at a
time; and what the current batch is. It is rewritten each time a batch is
chosen and it carries the record of the previous ones.

Six plans are complete and in `tasks/Archive/`. **Five are open with sixteen
tasks between them** — `strings-modernised` (6), `language-hygiene` (3),
`namespace-migration` (3), `ownership` (3) and `determinism` (1).
Two of those sixteen are owner-run smoke tests rather than agent work:
`determinism` T6, and `ownership` T6 needs one after each of its commits.

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
  way.** The most recent owner-reported successful run is `1af4979`
  (2026-08-05); the last run with an explicit all-seven-steps breakdown is
  `b0bde71` — see *What working looks like*. CI builds and runs the unit suite;
  it does not launch the client, and neither does any agent working on Linux. A
  change that compiles and passes 185 tests can still break the game on the
  first frame.
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
  `tasks/determinism.yaml` T3 and T4 are the reading that settled it.
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
    and for the same reason. **`determinism.yaml` T6 is the owner-run Garden
    smoke test that confirms it, and it is open.** Until it closes, T5 is
    landed but unverified against a running game.
  - **Nobody has swept the remaining LCG sites.** 186 of them were classified
    far enough to find the six; a site-by-site record of which feed simulation
    state does not exist and has no owning task.
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
- **The test suite is thin.** Four projects, **185** tests at `a676611`
  (2026-08-05), covering IP conversion,
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
  were deleted, notices included, and it explains why each row went.
