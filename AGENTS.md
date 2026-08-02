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
| How work is broken down | [`docs/TASK_DAG.md`](docs/TASK_DAG.md) |

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

> **Note:** the most recent commit is titled *"Cleanup done, But does not work
> yet"*. Assume the game does not currently run. Do not report a change as
> working on the basis of a successful compile.

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
NeuronServer/     ~0      Authoritative simulation host. A stub — header only.
GameLogic/        ~48k    Entities, buildings, teams, unit behaviour, in-game
                          windows. The bulk of the inherited code. Static library.
Species/          ~32k    Client executable: app, world, camera, landscape,
                          task manager, level loading.
Server/           ~0      Server executable. A stub — WinMain only.

tools/                    The checks CI runs. Run them locally too.
tasks/                    Task DAGs. See docs/TASK_DAG.md.
docs/                     Architecture, build, task breakdown standard.
```

`NeuronServer` and `Server` being empty is not an oversight — the server has not
been written. `NeuronCore` currently cannot be linked without the client and game
layers, which is the first thing standing in the way.

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

**Includes may only ever point downward.** The tree does not obey this yet: 663
upward includes are recorded in `tools/layering_allowlist.txt`, inherited from
Darwinia's single-binary layout.

| From | Into | Count |
|---|---|---|
| GameLogic | Species | 586 |
| NeuronClient | Species | 42 |
| NeuronCore | NeuronClient | 11 |
| NeuronCore | Species | 10 |
| NeuronCore | GameLogic | 9 |
| NeuronClient | GameLogic | 5 |

The check fails on any violation **not** already in that file. The allowlist may
only ever shrink. If your change needs a new upward include, the design is wrong:
move the shared declaration down into a layer both sides can see, or invert the
dependency behind an interface.

`tasks/neuroncore-layering.yaml` is the plan for eliminating the 30 `NeuronCore`
entries, which unblocks a headless server build.

---

## Building

```powershell
msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64 /m
```

Visual Studio 2026 (toolset v145), C++20, Windows-only. ARM64 is the primary
development platform; x64 is supported and built in CI. Full detail, including
the x64 story and the known PCH quirk, is in [`docs/BUILD.md`](docs/BUILD.md).

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

Then build Debug for at least one platform — CI does the same on both. A change
that has not been compiled is not finished. Build Release too before anything
that ships; CI does not.

**The project-file check matters more than it looks.** Adding a `.cpp` without
adding it to the `.vcxproj` produces no error — the file is simply never
compiled, and the symbols go missing at link time with no indication why.

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

**Never add to the layering allowlist.** It exists to shrink.

**Do not reformat files you are not otherwise changing.** Formatting is enforced
on changed lines only, deliberately: a repo-wide reformat would destroy `git
blame` across 113,000 lines. Whole-file formatting is a migration task, done
deliberately, one file at a time, in its own commit.

**Say when something did not work.** A compile is not a test. This project has no
test suite yet, so the honest report is often "it builds; I could not verify it
runs". Write that, rather than implying more.

**Ask before changing build topology.** Adding a project, changing the toolset,
adding a dependency, or restructuring the solution affects everyone's build. It
warrants a question first.

---

## Known issues

Real, currently true, and worth knowing before you trip over them:

- **The game does not run.** Last known state per the HEAD commit message.
- **ARM64 Debug has an unresolved failure.** CI builds x64 Debug only, so the
  primary development platform is not gated. One CI run built ARM64 Debug and
  failed after `NeuronClient.lib` linked — meaning the break is in `GameLogic`
  or `Species` — but the error scrolled past the retrievable log tail and was
  never isolated. A later run got much further without failing, so it may have
  been a flake. **If you develop on ARM64 and hit a Debug build error, this is
  known and unexplained; capture the first error and record it here.**
- **`NeuronCore` depends upward** on `NeuronClient`, `GameLogic` and `Species`,
  including reaching through the `g_app` global. It cannot be linked standalone.
- **Release had never built, and CI does not gate on it.** Three template
  leftovers — missing include paths, a precompiled header nothing created, and
  `Species` linking Release as a console app when `WinMain` is its entry point —
  are all fixed. The first two were confirmed fixed by a CI run; the third was
  applied after CI dropped to Debug-only, so **Release has not been built since**.
  Build it locally before anything that ships. Details in
  [`docs/BUILD.md`](docs/BUILD.md).
- **`NeuronCore.h` still carries Darwinia's target macros** (`TARGET_FULLGAME`,
  `TARGET_DEMOGAME`, `DARWINIA_VERSION`, and a `#error` if none is defined), plus
  `TARGET_OS_LINUX` and `TARGET_OS_MACOSX` branches for platforms that are not
  built. Most of it is dead.
- **There is no test suite.** `Species/TestHarness.cpp` is dead code behind
  `TEST_HARNESS_ENABLED`, and is a level-progression explorer rather than a unit
  test framework.
- **No `LICENSE` file.** Given the Darwinia provenance, the repository should
  carry one. That is the owner's call, not an agent's.
