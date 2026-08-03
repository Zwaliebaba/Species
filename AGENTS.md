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

Species is a C++20 game built on the **Neuron** engine. The long-term goal is a
large-scale realtime multiplayer world in which player colonies work, live and
survive persistently.

It is not that yet. The codebase began as the Darwinia source and is partway
through being reshaped: renamed, relayered, and modernised into something a
persistent authoritative server can be built on. Roughly 113,000 lines of C++
across six MSBuild projects, with no third-party dependencies — it links only
against the OS (OpenGL, GLU, WinMM, DirectSound, Winsock).

**Do not treat the ambition as the current scope.** The realtime world does not
exist. Nothing in the tree is designed for it yet. Work toward it happens by
making the existing code capable of supporting it, not by building it alongside.

---

## Current priority

**Cleanup and modernisation first.**

The near-term goal is to finish converting the inherited Darwinia code into
Neuron-style C++20 with enforced layer boundaries. A runnable game and the
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

**This phase is done.** `tasks/neuroncore-layering.yaml` is the plan that got
there; all thirteen of its tasks are complete.

What that does *not* mean: the game client runs, the world server exists, or
cross-architecture play works. It means the foundation no longer depends on the
things above it, so a server can be built without dragging a renderer in.

Deliberately *not* the exit criterion: the rest of the tree's upward includes,
and the client running. Both matter; neither gated the world server. The
allowlist stood at 628 when this phase ended and is **gone** —
`tasks/layering-inversion.yaml` took it to zero and deleted it.

> **Note:** the game runs again as of `7ee8c00`. That is recorded here because
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

NeuronCore/       ~3.4k   Foundation: sockets, threads, byte streams, the wire
                          protocol, filesystem, assertions. Static library.
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
tasks/                    Task DAGs. See docs/TASK_DAG.md.
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
`tasks/layering-inversion.yaml` removed them over eighteen tasks and deleted the
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

`tasks/neuroncore-layering.yaml` is the plan that eliminated the `NeuronCore`
entries. Twelve of its thirteen tasks are done; only T10 is left, which drops the
upward include paths from `NeuronCore.vcxproj` and makes `Server.exe` tick.

---

## Building

```powershell
msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64 /m
```

Visual Studio 2026 (toolset v145), C++20, Windows-only. ARM64 is the primary
development platform; x64 is also supported, and x64 Debug is the one
configuration CI builds. Full detail — configurations, what CI does not cover,
troubleshooting — is in [`docs/BUILD.md`](docs/BUILD.md).

---

## Before you push

Run all five. CI runs the same five and will fail on anything you skip.

```bash
python3 tools/check_project_files.py   # .vcxproj matches the files on disk
python3 tools/check_layering.py        # no new upward includes
python3 tools/check_task_dag.py        # task plans are valid DAGs
python3 tools/check_containers.py      # no legacy container call left on a vector
python3 tools/check_format.py          # changed lines match .clang-format
python3 tools/check_hygiene.py         # changed lines do not reintroduce NULL,
                                       # _included guards, strcpy or plain enum
```

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
reason — there are two in the tree today, and both are explained in
`tasks/language-hygiene.yaml` T1.

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

**Last full run: all seven steps pass, as of the layering-inversion branch
(2026-08-02), after the `g_app` seam moved the world subsystems, the frame
clock and App's state out of the executable.** Reported by the project owner,
not observed by the agent that wrote this line — if only some steps were
checked, correct this rather than leaving it overstated.

**Partial run at `586c072` (2026-08-03), after containers-replaced T12
converted the world's slot containers off `DArray` and the entity rename
landed.** The owner launched, loaded The Garden and played without an assert —
steps 1, 2, 5, 6 and 7. **Steps 3 and 4, the spawn counts, were not checked**,
so this is not a full pass and does not close `rename-darwinian/T4`. That
distinction is worth keeping: the counts are the only step that catches a
string-resolved reference the rename missed, because a name that fails to
resolve produces a smaller group rather than a crash.

That run is the reason those changes were merged: CI proved they compile and
the unit suite passes, and neither says anything about whether the game still
starts, spawns and advances.

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
way to record an exception, which is the point — `tasks/layering-inversion.yaml`
has eighteen worked examples of removing one properly. The same check also
catches a symbol declared in a library header and defined only in an executable,
which is the same reach with the linker doing the work instead of the
preprocessor.

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
  way.** All seven Garden smoke-test steps passed as of `7ee8c00` — see *What
  working looks like*. CI builds and runs the unit suite; it does not launch the
  client, and neither does any agent working on Linux. A change that compiles and
  passes 37 tests can still break the game on the first frame.
- **The sound system draws from the simulation's random stream.**
  `NeuronClient/SoundInstance.cpp` calls `speciesRandom()` twice — line 533 to
  pick a sample from a group, line 1009 to pick an object id. That is the
  deterministic lockstep RNG, the same sequence `Location::Advance` consumes,
  and `CODING_STANDARDS.md#determinism` names the `speciesRandom()` call
  sequence as load-bearing.
  - The consequence is worse than it first reads. Whether those lines execute
    depends on the sound path: how many object ids an instance holds, whether a
    sample group is populated, whether the instance is playing at all. Two
    clients that differ in sound configuration — or that fail to load the same
    samples — draw a different NUMBER of values from the shared stream, and
    every subsequent `speciesRandom()` in the simulation returns something
    different on one machine than the other. That is a desync, and nothing in
    the build or the test suite would show it.
  - **Not investigated, not reproduced, and not fixed** — found by reading
    while scoping `containers-replaced` T5 on 2026-08-02. It may already be
    benign for reasons the code does not state (if these paths run identically
    on every client regardless of settings, it costs nothing). Establishing
    which is true is worth doing before multiplayer is trusted, and it is a
    determinism question rather than a modernisation one.
- **Cross-architecture play is unproven.** The projects build ARM64 and x64 with
  MSVC float defaults — no `<FloatingPointModel>` is set anywhere in the tree.
  Deterministic lockstep requires bit-identical results, and nobody has verified
  that an ARM64 client and an x64 client agree, given FMA contraction and the 281
  `sinf`/`cosf`/`powf` calls in simulation code. Assume they desync until tested.
  Now that the game runs this is finally testable: two clients, one per
  architecture, and watch for the sync assert in `Server.cpp`.
- **ARM64 Debug is not gated by CI.** CI builds x64 Debug only, so the primary
  development platform is never checked here — build it yourself before relying
  on it. The unexplained ARM64 Debug failure that used to be recorded in this
  spot has not recurred and is now attributed to the C3859 memory pressure
  described below; it is written up there as a resolved instance rather than
  kept as a standing mystery.
  - Adding ARM64 to CI was proposed and **declined on 2026-08-02**: the arm64
    runner is a preview image that roughly doubles wall clock, and ARM64 is built
    constantly at the desk anyway. Deliberate, not an oversight.
- **No upward includes remain, down from 628.** `tasks/layering-inversion.yaml`
  is complete and `tools/layering_allowlist.txt` is **deleted** — not emptied,
  deleted, so nobody can reopen it by adding a line. `NeuronCore` also lists no
  include directories at all in its `.vcxproj`, so an upward include there fails
  to compile rather than quietly working. `App.h` is now in the same position for the layers below it: the
  subsystem pointers live in `NeuronClient/WorldPointers.h`, the application
  state in `AppState.h`, and the seven actions only `App` can perform behind the
  `AppCommands` interface it installs at startup. `Renderer`, `Camera`,
  `Script`, `UserInput`, `TaskManagerInterface`, `ControlHelp`,
  `LocationEditor` and `GameCursor` are each reached through an `*Access`
  interface header in `NeuronClient`, and the world model — `Location`,
  `GlobalWorld`, `Team`, `Unit`, the grids, the routing system and the
  landscape — now lives in `GameLogic` rather than being reached up into.
  `check_layering.py` also reports any free function or extern variable a
  library header declares that only an executable defines — an upward reach the
  linker resolves and an include check cannot see. The tree carried one for
  years: `WindowManager.h` declared `AppMain()` and `Species` defined it (T18).
  Class members are exempt, because a pure-virtual declared low and overridden
  high is dependency inversion, which is how most of the 628 were removed.
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
- **The test suite is thin.** Four projects, 103 tests, covering IP conversion,
  the `speciesRandom` sequence, the `ByteStream` macros, both halves of the wire
  format (`NetworkUpdate` and `ServerToClientLetter`), the `FilesysUtils` path
  helpers, `WorldObjectId` including its 16-byte wire layout, the state a new
  `Server` starts in, the legacy containers plus their `Neuron::SlotMap`
  replacement, and the preferences file format. That is the encoding, identity
  and protocol layer and almost nothing else — no entity behaviour, no
  rendering, no level loading, and nothing at all that would notice the game
  failing to start.
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
  Entity and building behaviour is finally testable — nobody has written those
  tests yet, which is `tasks/layering-inversion.yaml` T11. `Species` and `Server` have no
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
  redistributed or used commercially without settling that first. Three files
  carry third-party notices that must never be stripped, including when moved or
  modernised; they are listed in `LICENSE`.
