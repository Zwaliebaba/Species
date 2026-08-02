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

1. `tools/layering_allowlist.txt` contains no entry beginning `NeuronCore/`.
   **Met** — zero, down from 30.
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
things above it, so a server can be built without dragging a renderer in. The
next phase starts above it: every one of the 628 remaining violations points
into `Species` (624 of them) or `GameLogic` (4).

Deliberately *not* the exit criterion: the full 628-entry allowlist (the
`GameLogic` → `Species` cluster is 584 of them and blocks nothing here), and the
client running. Both matter; neither gates the world server.

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

**Includes may only ever point downward.** The tree does not obey this yet: 628
upward includes are recorded in `tools/layering_allowlist.txt`, inherited from
Darwinia's single-binary layout.

| From | Into | Count |
|---|---|---|
| GameLogic | Species | 584 |
| NeuronClient | Species | 40 |
| NeuronClient | GameLogic | 4 |

**`NeuronCore` has no upward includes left.** Every remaining violation is in
`NeuronClient` or `GameLogic` reaching into `Species`.

The check fails on any violation **not** already in that file. The allowlist may
only ever shrink. If your change needs a new upward include, the design is wrong:
move the shared declaration down into a layer both sides can see, or invert the
dependency behind an interface.

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

Run all four. CI runs the same four and will fail on anything you skip.

```bash
python3 tools/check_project_files.py   # .vcxproj matches the files on disk
python3 tools/check_layering.py        # no new upward includes
python3 tools/check_task_dag.py        # task plans are valid DAGs
python3 tools/check_format.py          # changed lines match .clang-format
```

`python3 tools/check_format.py --fix` applies the formatting rather than
reporting it.

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
| 3 | **50 Darwinians** spawn on team 0 — two groups of 30 and 20 | Entity creation from `InstantUnits` |
| 4 | **179 Virii** spawn on team 1 across eight groups | Multi-team spawning |
| 5 | Both move under their own behaviour for 30s with no assert | `Location::Advance`, the slice loop, entity AI |
| 6 | The Task Manager opens and lists available programs | Eclipse UI and `GlobalResearch` |
| 7 | `GenerateSyncValue()` does not trip the sync assert in single player | The simulation is at least self-consistent |

Those counts are read from `MissionGardenLiberate.txt`, so they are checkable
rather than approximate. Any step failing localises the break to a subsystem.

**Last run: all seven steps pass, as of `7ee8c00` (2026-08-02).** Reported by the
project owner, not observed by the agent that wrote this line — if only some
steps were checked, correct this rather than leaving it overstated.

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

**Never add to the layering allowlist.** It exists to shrink. The one
exception is a file rename, which orphans its entries and makes unchanged
violations look new — rewrite those with
`python3 tools/check_layering.py --rename OLD NEW`, which cannot invent
entries and leaves the count untouched.

**Do not change what the simulation computes.** Multiplayer is deterministic
lockstep with a runtime checksum, so iteration order, floating-point arithmetic
order, container choice and the `speciesRandom()` call sequence are all
load-bearing. A refactor that looks purely cosmetic can desync the game while
every build stays green. `DArray` in particular is a slot map whose indices are
network identity — it is not a `std::vector`. Read
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
- **`NeuronClient` and `GameLogic` still reach up into `Species`.** 627 upward
  includes remain, and the direction that mattered most is already fixed:
  `NeuronCore` is standalone, reaches upward nowhere, and `NeuronCore.vcxproj`
  lists no include directories at all, so a new upward include there fails to
  compile rather than quietly working. The remaining debt is the next phase, and
  it is the reason `Tests/GameLogicTests/LinkStubs.cpp` has to exist.
- **Release is not built by anyone.** Three template leftovers — missing include
  paths, a precompiled header nothing created, and `Species` linking Release as a
  console app when `WinMain` is its entry point — are all fixed, and
  `Species.vcxproj` now sets `SubSystem` to `Windows` in both configurations. But
  the first two were confirmed by a CI run and the third landed after CI dropped
  to Debug-only, so **Release has still not been built since**. CI does not gate
  it and nobody builds it by hand, which means it can be broken right now and
  nothing would say so. Build it locally before anything that ships. Details in
  [`docs/BUILD.md`](docs/BUILD.md).
- **The test suite is thin.** Four projects, 45 tests, covering IP conversion,
  the `speciesRandom` sequence, the `ByteStream` macros, both halves of the wire
  format (`NetworkUpdate` and `ServerToClientLetter`), the `FilesysUtils` path
  helpers, `WorldObjectId` and the state a new `Server` starts in. That is the
  encoding, identity and protocol layer and almost nothing else — no entity
  behaviour, no rendering, no level loading, and nothing at all that would notice
  the game failing to start.
  `GameLogic` can only be linked into a test DLL through
  `Tests/GameLogicTests/LinkStubs.cpp`, which stands in for the `Species`
  globals it reaches up for and may only shrink. `Species` and `Server` have no
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
