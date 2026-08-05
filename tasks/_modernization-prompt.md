# Prompt: author the full modernization task DAG

Copy everything below the rule into a fresh agent session on this repository
(branch of your choice). It produces plan files under `tasks/`, not code
changes. The figures embedded in it were measured on 2026-08-02 at `b69c951`;
the prompt instructs the agent to re-measure rather than trust them.

---

You are working in the **Species** repository, a ~113k-line C++20 Windows game
(six MSBuild projects, no third-party dependencies) partway through being
modernised from the inherited Darwinia source into Neuron-style C++20 with
enforced layer boundaries.

## Mission

Produce a **complete modernization plan for the entire codebase**, expressed as
one or more task-DAG YAML files under `tasks/`, conforming to the schema in
`docs/TASK_DAG.md` and validating clean under `python3 tools/check_task_dag.py`.

This session is **planning only**. You write, validate and commit plan files.
You do not convert any code, and you do not start executing tasks. The plan you
write must be executable incrementally, by multiple agents in parallel, over an
extended period — that is what the DAG structure is for.

## The inviolable constraint

**The game's observable behaviour must not change.** Every task you write is a
refactoring task; none may alter what the simulation computes. This repository
is deterministic-lockstep multiplayer with a runtime sync checksum:
`GenerateSyncValue()` sums entity positions and velocities in container index
order, so **iteration order, container identity, floating-point arithmetic
grouping, the `speciesRandom()` call sequence, and `DArray` slot indices (which
are network identity via `WorldObjectId`) are all load-bearing.** A refactor
that looks cosmetic can desync the game while every build stays green.

Read `CODING_STANDARDS.md#determinism` in full before writing a single task
that touches `GameLogic/` or anything in `Species/` reachable from
`Location::Advance`. Every task over simulation code must carry an explicit
determinism note in its `intent` or `notes`, and its acceptance criteria must
include the characterization tests that prove behaviour was preserved.

## Step 0 — read before planning

In this order, completely:

1. `AGENTS.md` — current priority, what is in and out of scope, known issues,
   decisions already made and declined.
2. `CODING_STANDARDS.md` — the target style, the migration stages, determinism.
3. `docs/TASK_DAG.md` — the plan schema you are writing to.
4. `docs/TESTING.md` — what earns a test, characterization-first conversion.
5. `docs/ARCHITECTURE.md` — layers, runtime model, where things live.
6. `tasks/Archive/neuroncore-layering.yaml` and `tasks/Archive/rename-scaffolding.yaml` — the
   two completed exemplar plans. Match their standard: empirically grounded
   summaries, counts cited with the command that produced them, observable
   acceptance criteria, honest notes.

## Step 1 — survey and re-measure

The documentation is explicit that counts drift: **re-measure everything you
cite.** Figures as of 2026-08-02 (`b69c951`), with the commands to reproduce:

| Axis | Measured | Command sketch |
|---|---|---|
| `LList` usage | 127 files (GameLogic 82, Species 26, NeuronClient 16, NeuronCore 2, NeuronServer 1) | `grep -rlw LList --include="*.cpp" --include="*.h" <proj>` |
| `DArray` usage | 14 files | `grep -rlw DArray ...` |
| `FastDArray` | 17 files | |
| `BTree` | 5 files | |
| `HashTable` / `SortingHashTable` | 7 files | |
| C string calls (`strcpy`, `strncpy`, `strcat`, `sprintf`, `snprintf`) | ~345 sites (GameLogic 132, Species 115, NeuronClient 113, NeuronCore 22) | `grep -rEo ...` |
| Raw `new` in `.cpp` | ~813 sites; ~246 `delete`; 35 `SAFE_DELETE` | |
| `NULL` | ~578 | `grep -rwo NULL ...` |
| Header guards | 223 of 242 headers use `#ifndef _included_*` (a reserved-form identifier), ~15 use `#pragma once` | |
| Plain `enum` in headers | 11 (`enum class`: 0) | |
| Layering allowlist | 628 entries: GameLogic→Species 584, NeuronClient→Species 40, NeuronClient→GameLogic 4 | `python3 tools/check_layering.py` |
| Frozen Darwinia-derived names | 372 occurrences, 27 spellings — see `CODING_STANDARDS.md` for why each group is frozen | |
| Test suite | 4 projects, 45 tests, protocol/encoding/identity only | |

Before writing tasks, skim representative files on each axis so the tasks are
shaped by what the code actually does — e.g. `LList` ownership semantics
(`EmptyAndDelete`), the fixed 42-byte packet buffers in the netcode, and
`g_app`'s subsystem ownership in `Species/App.cpp`.

## Step 2 — the axes the plan must cover

Cover **every component**: `NeuronCore`, `NeuronClient`, `NeuronServer`,
`GameLogic`, `Species`, `Server`, `Tests/`, and `tools/` where the checks need
strengthening. The migration-stage table in `CODING_STANDARDS.md` says stages
1, 2, 6, 7, 8 are done and stages 3–5 are open; those three stages are the
backbone, but they are not the whole plan. Address all of the following, and
anything else you find — be exhaustive in the survey even where you recommend
deferring:

**A. Containers (stage 3).** `LList` → `std::vector`/`std::list`; `BTree`,
`HashTable`, `SortingHashTable`, `FastDArray`, `2dArray`, `BoundedArray`,
`AutoVector` → standard equivalents. Split simulation-state usages from
UI/cache usages: unordered containers are forbidden for simulation state,
fine elsewhere. **`DArray` is a slot map whose indices go on the wire — it is
never replaced by `std::vector`.** Plan it as a design task (modernise the
template in place, or write a Neuron slot map with identical index semantics)
and make that a decision point (see Step 4). Preserve iteration order
everywhere; preserve `LList` ownership semantics or make ownership explicit in
the same task's characterization tests.

**B. Strings (stage 4).** `char*`/`char[N]` buffers, `strcpy`/`sprintf` family
→ `std::string`/`std::string_view`/`std::format`. The largest latent-bug
surface. Exclusions to plan around: buffers that ARE wire format (fixed
42-byte packets, `ByteStream` macros) keep their layout; anything serialised
gets a byte-pinning test before it is touched.

**C. Ownership (stage 5).** Raw `new`/`delete`, `SAFE_DELETE`,
`EmptyAndDelete` → `std::unique_ptr` and values. Depends on stage 3 per file
(ownership currently lives inside `LList`). `App.cpp`'s subsystem graph is the
big win and the big risk; plan it late, in small steps, behind
characterization.

**D. Language hygiene.** `NULL` → `nullptr`; C casts → `static_cast`; plain
`enum` → `enum class` (careful: some enums are serialised as ints — pin bytes
first); `#ifndef _included_*` guards → `#pragma once` (also fixes 223 uses of
reserved-form identifiers); `const`/`constexpr`/`[[nodiscard]]`; range-`for`
over index loops (never in simulation traversals where order could shift).
Decide which of these ride along with a file's conversion task versus which
are safe mechanical tree-wide sweeps — a sweep touches every file and is
`parallel_safe: false`.

**E. Namespaces and naming.** Engine code belongs in `namespace Neuron`;
`GameLogic`, `Species` and most of `NeuronClient` are at global scope. Plan
the migration or explicitly defer it with reasoning. **Frozen renames stay
frozen:** anything named in `GameData/` (`Darwinian`, …), game-name strings,
save paths. The `Darwinian`→entity rename exists as a known, unblocked-but-
unwritten task gated on the owner running the Garden smoke test — include it
only as a clearly gated, owner-verified task, or exclude it with a note.
Renames that do land use the `Rename:` commit trailer and
`check_layering.py --rename` (see `CODING_STANDARDS.md`).

**F. Layering debt.** 628 upward includes remain: GameLogic→Species 584
(`App`, `Location`, `Team`, `GlobalWorld`, `LevelFile`), NeuronClient→Species
40, NeuronClient→GameLogic 4. `AGENTS.md` calls this "the next phase". Decide
— and ask, see Step 4 — whether it is a sibling plan or part of this one.
Either way, plan at least the moves that unblock modernization itself:
shrinking `Tests/GameLogicTests/LinkStubs.cpp`, and moving testable code out
of the `Species`/`Server` executables into libraries (an `.exe` cannot be
linked into a test DLL). The allowlist only ever shrinks; no task may add to
it.

**G. Testability.** Every conversion is characterization-first: a test
written against the legacy behaviour, in its own commit, still passing after
the conversion (`docs/TESTING.md`). Where code genuinely cannot be tested yet
(the `GameLogic` link wall), the task's `notes` must say so — plan the
enabling work (F) so the wall shrinks over time. Wire format and determinism
constants are pinned by value, always.

**H. Dead code.** Commented-out Darwinia blocks, `#if 0` regions, unbuilt
platform branches (`NetLibApple.h`). Removal is fair game during a file's
conversion; plan any standalone deletions explicitly, and ask before deleting
anything that looks like an unfinished feature rather than a leftover.

**I. Formatting migration.** Whole-file `.clang-format` conversion is legal
only as part of a file's deliberate conversion (`check_format.py --all
<file>`) — never repo-wide. Fold it into each conversion task rather than
planning a sweep.

**J. Build, tools and CI.** In scope per `AGENTS.md` ("tightening project
structure, improving the build and the checks") — e.g. checks that would catch
regressions on the axes above, or gaps you find in the four existing tools.
**Ask before anything that changes build topology** (new project, toolset,
dependency, solution restructure). Do **not** re-propose ARM64 CI or Release
CI; both were declined on 2026-08-02 and the reasoning is recorded in
`AGENTS.md`.

**Out of scope — do not plan it:** new gameplay, the persistent world,
netcode redesign, renderer rewrites, cross-architecture determinism auditing
(record it as a known risk, as `AGENTS.md` already does). If you believe
something out of scope must be done first, say so in the plan summary rather
than smuggling it in as tasks.

## Step 3 — how to shape the DAG

- **One reviewable diff per task.** File-by-file for stages 3–5 conversions,
  or a small coherent group. A 127-file `LList` migration is many tasks, not
  one; group by subsystem so waves are wide.
- **Characterization before conversion**: where a file's conversion needs
  tests first, the test node is a separate task the conversion depends on —
  the documented exception to tests-ship-with-the-change.
- **Stage order holds per file**: a file finishes stage 3 before starting 4,
  before 5. Different files may be at different stages concurrently. Express
  this with edges only where the same file is touched — do not serialise
  independent files for tidiness.
- **Declare `files`** on every task so collisions between concurrent tasks
  are visible. Tree-wide sweeps are `parallel_safe: false`.
- **Acceptance must be observable and preferably countable** ("`grep -cw
  LList GameLogic/Unit.cpp` returns 0", "allowlist shrinks by 9"), never
  aesthetic ("is cleaner").
- **`verify` lists real commands**: the four `tools/` checks always;
  `msbuild` + `vstest` for anything that compiles. Note that agents on Linux
  can run only the Python checks — the plan still lists the Windows commands
  because CI runs them; executing agents must report honestly which they ran.
- **Every task starts `status: todo`.** Statuses change only during
  execution, per the `docs/TASK_DAG.md` protocol.
- Respect the standing rules while shaping tasks: the four-edit `.vcxproj`
  rule for any file add/move; `pch.h` first in every `.cpp`; the three
  third-party licence notices travel verbatim with their files
  (`AutoVector.h`, `MathUtils.cpp`, `TriTri.cpp` — listed in `LICENSE`);
  modernisation commits contain nothing but the conversion.

Prefer **several plan files, one per axis** (e.g.
`containers-replaced.yaml`, `strings-modernised.yaml`, `ownership.yaml`,
`language-hygiene.yaml`, …) over one monolith, with cross-plan ordering
stated in each `summary` — unless the answer to the granularity question in
Step 4 says otherwise. Run `check_task_dag.py --waves` on each finished plan
and sanity-check that early waves are wide (parallelism is the point) and
that nothing in wave 1 depends on an unwritten decision.

## Step 4 — ask before you finalise

Some choices are the owner's, not yours. If a question can be answered from
the tree or the docs, answer it yourself and cite where; ask only what they
leave genuinely open. Ask via `AskUserQuestion` if available; otherwise put
an **Open questions** section in the plan `summary` and mark dependent tasks
`blocked` with a note. At minimum, resolve these:

1. **Plan granularity** — several per-axis plans (recommended) or one file?
2. **Layering inversion scope** — is the GameLogic→Species cluster (584
   entries) part of this modernization effort or a separate follow-on plan?
   It gates testability of everything in GameLogic, so the answer changes
   the dependency structure of axes A–C and G.
3. **`DArray` strategy** — modernise the template in place (keep name and
   semantics) or introduce a new Neuron slot map and migrate callers?
   Both preserve index semantics; they differ in churn and in wire-format
   audit burden.
4. **Namespace migration** — move game code into `Neuron` (or a `Species`)
   namespace as part of this effort, or defer?
5. **The `Darwinian` rename** — include as gated tasks requiring an
   owner-run Garden smoke test, or exclude from this plan?
6. **Sweep appetite** — are mechanical tree-wide passes (`NULL`→`nullptr`,
   guards→`#pragma once`) acceptable as single large diffs, or should they
   ride along file-by-file with conversions? (Note the changed-lines format
   check interacts with sweeps; see the rename-trailer mechanism.)
7. **Execution start** — once plans validate, should agents begin executing
   wave 1, or does the plan await owner review first?

## Step 5 — deliverables

1. `tasks/<plan>.yaml` file(s): valid (`python3 tools/check_task_dag.py`
   exits clean), every task `todo`, summaries carrying your re-measured
   counts and the commands that produced them.
2. A closing report: total tasks per plan, wave structure (`--waves`
   output), the open questions and what you assumed pending answers, and
   anything surprising the survey turned up.
3. Commit the plan files (and nothing else) to your working branch with a
   clear message. Do not push to `main`.

Honesty rules apply throughout: report what you actually ran; a compile is
not a test; on Linux you cannot build or run the game — say so rather than
implying you checked.
