# The next implementation batch

> ## BATCH 5 IS EXECUTED. All four of its agent tasks landed on 2026-08-05:
> `ownership` T11, `language-hygiene` T10 and T13, and `strings` T18. What
> happened is at [**Progress — what Batch 5 did**](#progress--what-batch-5-did),
> and it is the section to read before proposing Batch 6. **The proposal below
> is kept as written, not rewritten to match the outcome** — the point of this
> file is that a batch proposal is a measurement with a short half-life, and
> the only way to see that is to leave the prediction next to the result.
>
> **Nothing here was compiled by the agent that wrote it** — no MSVC and no
> Windows client in that environment. **CI then rejected it**, and the fix and
> what it teaches are at [**What CI caught**](#what-ci-caught). Three of the
> four tasks were green on CI as they landed; `language-hygiene/T10` was red
> on two errors and is green after a follow-up.
>
> **AND THEN THE OWNER RAN THE GAME.** All seven Garden steps pass at
> `acf283b`, 2026-08-05. That closes `determinism/T6` and its whole plan, and
> because the run was on this branch rather than on `main` it is also the
> first running-game evidence for all four of Batch 5's tasks. See
> [**What the smoke test settled**](#what-the-smoke-test-settled).

Written 2026-08-05 at `50560ab`. This is a proposal, not a plan — the plans are
the five YAML files beside it. It answers one question: **of everything that is
ready, what should the next batch be, and why that rather than the rest.**

Read [`AGENTS.md`](../AGENTS.md) first. [`_restart.md`](_restart.md) is the
modernisation reading order; it is still current and this file does not
supersede it. **Batches 1 to 4 are done** — what they were and what they taught
is at the end of this file.

> **This supersedes the Batch 5 proposed at `883d7a6`, which was never
> executed.** No task status has moved since it was written: the ready set is
> the same ten tasks, and all five checks that mattered still pass. What
> changed is the measurement. Three of that proposal's claims were wrong on
> contact, in the direction the previous four batches were each wrong in, and
> the batch below is the corrected version rather than a new subject. The
> numbering stays at 5 because nothing has been executed under it.

---

## Where every plan stands

Counted from the YAML at `50560ab`. Six plans are complete and in
`tasks/Archive/`; five are open with **sixteen tasks** between them.

| Plan | done | todo | What is left |
|---|---:|---:|---|
| the six archived plans | 87 | 0 | — |
| `determinism` | 5 | 1 | **T6 only, and it is yours** — the smoke test gating T5's RNG change |
| `ownership` | 8 | 3 | T11, then T6 (yours) → T7. Stage 5 ends here. |
| `language-hygiene` | 10 | 3 | T10 → T11, and T13 |
| `namespace-migration` | 2 | 3 | T2 → T4 → T5, and T5 waits on `ownership/T6` |
| `strings-modernised` | 14 | 6 | the largest remaining plan |

All seven local checks pass at `50560ab`, and CI is green on it.

> `check_format` compares against `origin/main`. A container that cloned before
> the last merge reports a failure that is entirely stale refs — `git fetch
> origin main` first, then re-run.

### Every open task, and whether it can be started

Sixteen tasks. `--next` calls ten of them ready; of those ten, two are owner
work and one is held by an edge `--next` cannot see.

| Plan | Task | Ready? | Blocked on |
|---|---|---|---|
| `determinism` | T6 — owner runs the Garden smoke test vs. the synchronised stream | **ready — OWNER** | nothing; needs a Windows client |
| `ownership` | T11 — unique_ptr in GlobalWorld | **ready** | — |
| `ownership` | T6 — App owns its subsystems | **ready — OWNER** | nothing in the graph; needs a per-commit smoke test |
| `ownership` | T7 — retire SAFE_DELETE / SAFE_FREE | no | `ownership/T6`, `ownership/T11` |
| `language-hygiene` | T10 — scope InputType | **ready** | — |
| `language-hygiene` | T13 — scope CamAnimNode's transition enum | **ready** | — |
| `language-hygiene` | T11 — scope ControlType | no | `language-hygiene/T10` |
| `namespace-migration` | T2 — NeuronClient into namespace Neuron | **ready** | — (sequenced last on purpose; see below) |
| `namespace-migration` | T4 — GameLogic into the game namespace | no | `namespace/T2` |
| `namespace-migration` | T5 — Species into the game namespace | no | `namespace/T4`, `ownership/T6` |
| `strings-modernised` | T11 — Eclipse widget name and caption members | **ready** | — |
| `strings-modernised` | T12 — the TextRenderer variadic text API | **ready** | — |
| `strings-modernised` | T17 — FileWriter's variadic printf | **ready** | — |
| `strings-modernised` | T18 — the last three variadic format entry points | **ready** | — |
| `strings-modernised` | T13 — narrow char const* params to string_view | no | `strings/T12` |
| `strings-modernised` | T9 — sweep the long tail to zero | no | eleven siblings; it is the plan's last node |

**Seven of the sixteen are startable by an agent today**: `ownership/T11`,
`language-hygiene/T10`, `language-hygiene/T13`, `namespace/T2`, and
`strings/T11`, `T12`, `T17`, `T18` — eight, less `namespace/T2`, which is ready
in the graph and should not be started, for the reason under *Not in the batch*.

---

## The finding that shapes this batch

**Three of the ten ready tasks have a file list that under-reports where they
have to edit, and the collision script in this file cannot see any of it.**

That script reads declared `files` lists. Batch 4 was bitten by exactly this —
`ownership/T5` declared eight files and touched twenty-seven, and CI found the
other nineteen. So this time the reach was measured first, by grepping what the
tree actually spells, for every task in the candidate set. Three lists were
wrong, and one of the three changes which tasks can share a batch:

| Task | Declared | Measured | What was missing |
|---|---:|---:|---|
| `ownership/T11` | 2 | **7** | five files hold range-for loops over the vectors it converts |
| `strings/T18` | 5 | **9** | four files hold 13 of the 16 `NetDebugOut` calls |
| `language-hygiene/T10` | 26 | 26 | nothing — the one list in the ready set that is exactly right |

The measurements are now in each task's `notes`, site by site, so the next
agent to open one of them does not have to repeat this.

### The collision the declared lists hid

**`ownership/T11` reaches `Species/App.cpp`, which is `ownership/T6`'s file.**
Neither list mentions it: T11 declares only `GlobalWorld.cpp/.h`, T6 declares
only `App.cpp/.h`, and the overlap exists because `App.cpp:331` and `:336`
iterate `g_globalWorld->m_buildings` and `m_locations` — the vectors T11
converts. Not a dependency in either direction, and no reason to add a graph
edge. But **T11 and T6 must not be in flight at the same time**, and T6 is the
owner-gated task everything downstream is waiting for, so that is worth knowing
before both get claimed on the same day.

The same measurement widens a collision the previous proposal had at one file:
**`ownership/T11` × `strings/T17` is four files, not one** — `GlobalWorld.cpp`
as declared, plus `Generator.cpp`, `Mine.cpp` and `TrunkPort.cpp`, all three
already on T17's list and all three reached by T11's loops.

### What did NOT get worse

Worth stating, because the corrections above are all in one direction:
`ownership/T11`'s 69 `g_globalWorld->m_research` sites — the number that makes
that task look large — are **all** arrow-observation through the member, which
`unique_ptr::operator->` serves unchanged. Every one compiles untouched.
`m_globalInternet` and `m_sphereWorld` are named nowhere outside
`GlobalWorld.cpp/.h` at all. The task is eight loops and a destructor, not
seventy call sites.

---

## The proposal

### Batch 5 — four agent tasks that are disjoint on MEASURED reach, and two gates to open

| | Task | Reach | Why it is in the batch |
|---|---|---:|---|
| 1 | `ownership/T11` — unique_ptr in GlobalWorld | 7 files | The half of T5 split out for reviewability. 23 raw `new`, 19 raw `delete`, five owning vectors, three subsystem members — all shapes T5 already converted, so it is the best-understood task in the ready set. Its eight breaking call sites are now enumerated in its notes. |
| 2 | `language-hygiene/T10` — InputType, and delete the dead `ControlTypes.cpp` | 26 files | The only ready task whose declared list survived measurement intact. Unblocks `lh/T11` (473 sites), the last node in that plan. Deleting `ControlTypes.cpp` takes `NULL` to zero tree-wide. |
| 3 | `language-hygiene/T13` — the transition enum and its wrong bound | 4 files | Small and self-contained. Its only contested file is `LevelFile.cpp`, with `strings/T17`, which is not in this batch. |
| 4 | `strings/T18` — the last three variadic format entry points | 9 files | **New to this batch, and it is in it because it was measured.** The previous proposal called it "the one slot a fourth agent could take without any measurement"; the measurement has now been done, it reaches nine files rather than five, and it is still disjoint from all three of the above. |

**All four can run at once.** Pairwise intersection of measured reach is empty
for all six pairs: T11 is `GameLogic` world + `App.cpp`; `lh/T10` is
`NeuronClient`'s input drivers and `ControlTypes.inc`; `lh/T13` is `LevelFile`,
`CameraAnimWindow` and `Camera.cpp`; `strings/T18` is `NeuronCore`'s networking
plus `DebugRender` and `Script.h`.

**Three of the four are flagged `parallel_safe: false`, and the flags are wrong
rather than the measurement.** They were set when the tree was more contested
than it is now. Do not silently flip them — measure, then update the flag in
the same commit as the work, with the measurement in notes. `lh/T10`'s note
already reads that way, and `ownership/T11`'s and `strings/T18`'s now do too.

**One scheduling constraint, from the finding above: do not run `ownership/T11`
while `ownership/T6` is in flight.** They share `App.cpp`. If T6 is claimed
first, T11 waits; if T11 is claimed first, it is a short task and T6 should
wait for it, because T6 wants a still tree more than T11 does.

### The two gates, and what they unlock

Both are yours, both are Garden smoke tests, and **they are not the same run**:

- `determinism/T6` tests the RNG change. It closes `determinism`.
- `ownership/T6` tests App's teardown, per commit. It unblocks `ownership/T7`
  — which ends stage 5 and can delete the two dead `EmptyAndDelete` helpers
  along with the macros — and `namespace/T5`.

Batch 1's gate closed two tasks in one run because they were the same seven
steps on the same build. These two are not: `ownership/T6` lands commits that
`determinism/T6` would then be testing on top of. If one run is to serve both,
run `determinism/T6` **first**, on what is in `main` now.

**Migration stage 5 cannot end without the second run.** That is still the
single most useful thing to know when planning around this batch.

### What stage 5 has left, measured at `50560ab`

```
EmptyAndDelete   ZERO call sites. Only the two transitional definitions
                 remain, in SlotMap.h and VectorUtils.h — both dead code now,
                 and both can go in T7.
SAFE_FREE        ZERO call sites.
SAFE_DELETE      17, all in Species/App.cpp, plus the NeuronCore.h definition.
```

### Not in the batch, and why

- **`strings/T17` (FileWriter's variadic printf).** Ready, and still the only
  task contesting anything: it holds both of the declared-list collisions and
  three of the four new measured ones. It wants a clear tree, and it is the
  obvious first task of Batch 6. One correction for whoever takes it — its file
  list names `Tests/GameLogicTests/LevelFileRoundTripTests.cpp`, **which does
  not exist under that or any name.** The tests its acceptance line means are
  `Tests/NeuronClientTests/FileWriterTests.cpp` — the byte-level `printf`
  tests, which are the ones that would catch a reinterpreted format string —
  and `Tests/GameLogicTests/LevelFileTests.cpp`. Fix the path when you take the
  task.
- **`strings/T11` and `T12`.** Ready and large: T12 is 225 call sites in 45
  files and splits into two populations needing different treatments; T11 is
  over the tree's most contended member name. Each is a batch's worth alone.
- **`ownership/T6`, `determinism/T6`.** Owner work, above.
- **`namespace/T2`.** Ready in the graph, and it should still not be started.
  It declares whole directories — `NeuronClient`, `GameLogic`, `Species` — so
  it contests every other task by construction, and a namespace change touches
  every file in three projects. Note what this batch buys it: after
  `ownership/T6`, `namespace` is a clean `T2` → `T4` → `T5` chain with nothing
  else in its way — the whole plan, in order. **It is the last plan standing**
  and wants planning as a project rather than as a batch entry.

---

## The collision check

Ten ready tasks across five plans. **On declared lists, two contested files; on
measured reach, seven.** Both numbers are below, because the difference between
them is the point.

Declared (reproduce with the script at the end of this file):

| Pair | Contested |
|---|---|
| `ownership/T11` × `strings/T17` | `GlobalWorld.cpp` |
| `language-hygiene/T13` × `strings/T17` | `LevelFile.cpp` |

Measured:

| Pair | Contested |
|---|---|
| `ownership/T11` × `strings/T17` | `GlobalWorld.cpp`, `Generator.cpp`, `Mine.cpp`, `TrunkPort.cpp` |
| `language-hygiene/T13` × `strings/T17` | `LevelFile.cpp` |
| **`ownership/T11` × `ownership/T6`** | **`App.cpp`** — invisible to the script; see above |
| every other pair | 0 |

Two cautions carry over. `strings/T12` declares 2 files against a reach of 45,
and `namespace/T2` declares directories, so both under- and over-report by
construction — neither is in this batch and neither was measured here. And the
third: **`parallel_safe` flags in this tree lag reality.** Three of this batch's
four are flagged `false` and are provably disjoint today. Trust a fresh
measurement over the flag, and correct the flag when you find it wrong.

---

## Progress — what Batch 5 did

Four tasks, six commits, 26 files. Every one of the four met its acceptance
except `ownership/T11`, which met three of four and says so.

### Where every plan stands now

Nine tasks open across three plans, down from sixteen. `determinism` and `ownership` both closed; migration stage 5 is finished.

| Plan | done | todo | What is left |
|---|---:|---:|---|
| `determinism` | 6 | 0 | **CLOSED 2026-08-05** — archived |
| `ownership` | 11 | 0 | **CLOSED 2026-08-05** — archived. Stage 5 is finished. |
| `language-hygiene` | 12 | 1 | T11 only — 473 sites, now unblocked |
| `namespace-migration` | 2 | 3 | T2 → T4 → T5, and T5 waits on `ownership/T6` |
| `strings-modernised` | 15 | 5 | the largest remaining plan |

**Both gates are closed, and nothing is waiting on the owner any more.**
`determinism/T6` passed. `ownership/T6` landed afterwards in four commits,
unblocking `ownership/T7` and `namespace/T5`.

**T6 is the one to hold at arm's length.** Its acceptance asked for the game
to reach the main menu after each of its four commits and that was NOT done —
CI compiled them, nothing ran them. Two findings from it are worth carrying
into any similar work:

- **`~App()` never runs.** `Main.cpp` calls `Finalise()` then `exit(0)`, which
  does not unwind, and nothing deletes `g_app`. All seventeen `SAFE_DELETE`s
  were unreached code, and the task's own intent said the opposite. **The plan
  entry was written from the grep, not from the code around it.**
- **The real risk was already-shared ownership.** `g_renderer` and
  `g_taskManagerInterface` are deleted and rebuilt at runtime by GameLogic on
  paths that do execute. The owner chose to route replacement through App.
  Before converting a member, grep for who else *deletes or reassigns* it, not
  just who reads it.

And one that argues for the whole exercise: converting ownership turned a
silent hazard into a build error. `AttractMode` has no header anywhere and its
`#ifdef` is never defined; a raw pointer member had hidden that for years, and
a `unique_ptr` member could not.

### What the batch got wrong about itself

The proposal above says the reach measurement was the thing that made this
batch safe. It was — and it was still incomplete in a way worth recording:

- **`ownership/T11` reached EIGHT files, not the seven measured.** The miss was
  `GlobalWorldEditorWindow.cpp`, and the cause is the same class of error the
  measurement was written to catch. The sweep greped MEMBER NAMES, so it found
  every file that touches `m_locations` or `m_buildings`. That file names no
  member: it calls `new GlobalLocation()` and hands the result to
  `AddLocation`. **A member-name sweep cannot see a file that only touches the
  object in flight.** Grep the ownership-transfer entry points too — `Add*`,
  `Set*`, `Register*`.
- **`language-hygiene/T13` reached SIX files, not the four declared**, and the
  two extra were a design decision rather than a clerical miss.
  `DropDownMenu::RegisterInt` takes an `int*`, which a scoped enum cannot be
  reached through, so the widget gained a typed binding beside it. The task's
  acceptance offered an out — keep the member an int and say why — and it was
  refused, because an int member puts `Camera.cpp`'s three switches back to
  comparing an int against foreign enumerators, which is the hazard T13 exists
  to close.
- **`strings/T18` reached the nine files measured but only needed seven.**
  Two were in the reach and needed no diff. Reach and edit-count are not the
  same number, and the measurement is still what told us that in advance.

### What else this batch learned

- **`GlobalEvent` had no destructor at all**, so every condition and every
  unexecuted action leaked with the event. Found by converting, not by
  looking. Vectors of `unique_ptr` fix it.
- **The use-after-move trap is not hypothetical in this file either.**
  `GlobalWorld::AddLocation` calls `m_sphereWorld->AddLocation(location->m_id)`
  *after* `push_back`, and `AddLevelBuildingToGlobalBuildings` writes through
  its building after it. Both are fine only because an observer is taken with
  `.get()` before the move. That is now three tasks in a row where this shape
  appeared — T5 twice, T11 twice.
- **`ownership/T11` did not fully meet its "no raw owning new or delete"
  line, and the task notes say so** rather than claiming it. Four of the six
  survivors are `GlobalEventCondition`'s `char*` string members, which are
  stage-4 work reaching the level-file writer where byte-identity is another
  task's proof. **No strings task owns those two members** — T19 converted
  `GlobalEventAction::m_filename` and stopped. That is an unowned gap, and it
  is the shape of thing Batch 4 gave owners to.
- **`ControlTypes.inc`'s hand-aligned table did not survive `lh/T10`.**
  136 of its lines changed, so the changed-lines format check reformatted
  them. `// clang-format off` would have preserved the columns and was
  rejected: **this tree has no formatting escape hatch anywhere**, by
  deliberate policy — the last `hygiene-ok` marker went in T12 — and adding
  the first one to save one table's alignment is a `.clang-format` policy
  decision rather than a call a single task should make. If the alignment is
  wanted back, that is the conversation to have.
- **`NULL` is now at a genuine zero.** Deleting the dead `ControlTypes.cpp`
  took the last one; the only hits left tree-wide are a diagnostic string
  literal and a comment describing the sentinel that used to exist.
- **`ENUM_HELPER` has its first user**, after being carried unused in
  `NeuronHelper.h` since it was written. Only its bitwise half means anything
  for a bit field, and `InputTypes.h` now says so at the invocation.

### What CI caught

**Three of the four tasks were green on CI as they landed** — `lh/T13` at
`67c5580`, `strings/T18` at `a01ca40`, `ownership/T11` at `be89687`, each
building x64 Debug and running the suite. **`language-hygiene/T10` was red**,
on exactly two errors and nothing else in the tree:

```
InputDriverWin32.cpp(451,12): error C2440: 'return': cannot convert from 'int' to 'inputtype_t'
InputDriverWin32.cpp(473,12): error C2440: 'return': cannot convert from 'int' to 'inputtype_t'
```

**Read it as a method failure rather than a typo.** The sweep was keyed on the
enumerator NAME, so it found all 217 sites that *spell* an `INPUT_TYPE_` and
was structurally blind to the two that do not. Changing a function's RETURN
TYPE makes every `return` in it a conversion site, and `return -1;` names
nothing a grep over enumerators can match.

**This is the same shape as the miss `check_math_types.py` was built for**
during the DirectXMath migration — *"a converted signature leaves its
parameter's uses behind exactly as a converted local does"* — and there is no
equivalent check for enum conversions. The rule for the next one: after
changing a type, enumerate the functions that RETURN it and read every return
statement, then the assignments INTO it. A sweep over enumerator names cannot
see a bare integer, by construction.

**The fix was not the obvious one, and the obvious one would have inverted
behaviour.** Both sites are `default: return -1; // Should never get here!`,
and mapping them to `INPUT_TYPE_FAIL` reads like an improvement. It is the
opposite: `InputType` is a bit field, so `-1` is every bit SET and passes a
`(type & x) == x` test for everything, while `INPUT_TYPE_FAIL` is zero and
passes it for nothing. The value is read in exactly that form, in
`InputDriver::getDefaultConditionID`. Both are now
`static_cast<InputType>(-1)` — the bit pattern preserved, the boundary
declared, and which value is *correct* on that unreachable path left open,
with no owning task. **A compile error is not a licence to pick a new value.**

Worth noting against this file's own premise: the reach measurement that
shaped Batch 5 was about WHICH FILES a task touches, and it was good for
that. It says nothing about which EXPRESSIONS inside those files a conversion
reaches. Both errors were in `InputDriverWin32.cpp` — a file the measurement
named correctly, and had counted 19 sites in.

### What the smoke test settled

**All seven Garden steps pass at `acf283b`, owner-reported, 2026-08-05.**

The proposal said to run `determinism/T6` on `main` so the RNG change could be
judged alone. It was run on this branch instead, and that turned out to be the
better trade: one run closed the gate AND put four CI-only tasks in front of a
running game for the first time. Steps 2, 5 and 6 are what would have caught a
mistake in `ownership/T11`'s GlobalWorld ownership or `lh/T10`'s input enum.

**The cost of combining them is real even though it did not bite.** Had a step
failed, the cause would have been ambiguous between the RNG change and four
unrelated conversions. It did not fail, so the ambiguity is moot — but a
future gate should not assume combining runs is free.

**`acf283b` is the new simulation baseline.** `determinism/T5` postdates
`1af4979` and shifts the `syncrand` sequence, so this build's sync value is
not the one `1af4979` produced. A pre-T5 client desyncs against a post-T5 one.

**What it did not cover**, and AGENTS.md now says so: three of T5's six fixed
sites — Spam, GodDish and Library — are not in The Garden. Laser fences and
incubators are, and `LaserFence.cpp:66` was the one outright desync of the six.

### What was NOT done, and should be read as a gap

**No runtime tests were added by any of the four tasks.** Three static_asserts
went in — the transition-name table, and InputType's bit relationships — and
those are real, but they are compile-time. Nothing in `Tests/` covers the
input layer, `GlobalWorld`, or the debug renderer, and this batch did not
change that. `GlobalWorld` is the awkward one: its constructor builds
`SphereWorld`, which asks `g_resource` for shapes, so it is not constructible
in a test DLL as it stands. That is a real obstacle and it has no owning task.

**Nothing was compiled locally.** Four tasks, 26 files, no build in the
executing environment — the seven Python checks and a use-after-move scan
were the whole of the pre-push evidence, and CI found what they could not.
That is the second time in five batches that CI has been the thing which
caught a type conversion's missed call sites; `ownership/T5` was the first.

**A green CI run is still not a running game.** `GlobalWorld` is on the
Garden smoke-test path at step 2 and the input layer at step 5, and nothing
in the suite renders or reads a key.

---

## Progress — what Batch 4 did

Six tasks landed: `determinism/T3` and `T4`, `ownership/T5`, `T8` and `T10`,
`strings/T20` and `language-hygiene/T12`. Neither plan it was organised around
closed, and both reasons were findings rather than slippage.

**THE DETERMINISM WORK WAS DONE TWICE, CONCURRENTLY.** A branch and `main` both
took `determinism/T3` and `T4` on the same day, without either knowing about
the other, and reached the same answer independently: `speciesRandom()` is not
the lockstep RNG, `syncrand()` is, and `MathUtils.h` has said so since the code
was inherited. `main`'s version went further — **six** simulation-state draws
on the unsynchronised generator where the branch found four — and is the one
that survives. The two it found and the branch missed came through
`frand()`/`sfrand()` rather than `speciesRandom()` directly, including
`LaserFence.cpp:66`, an outright desync. The branch's own proposal had written
*"ONE TRAP: `frand()` and `sfrand()` are speciesRandom in disguise … a grep for
the two names is not optional"* and then did not run that grep.

**The scheduling lesson.** Task claiming is per-plan and advisory —
`status: in_progress` committed to a *branch nobody else fetches* prevents
nothing. Both agents claimed correctly and still collided. If two agents may be
working, the claim has to land on `main` before the work starts.

### What else Batch 4 learned

- **A file list is written from where ownership LIVES; the work is wherever the
  member is NAMED.** `ownership/T5` declared eight files and touched
  twenty-seven; twelve hold no ownership at all. **CI caught what the sweep
  missed** — eighteen call sites in eight files, every one reached through an
  expression the sweep had not thought to grep for: `m_tiles` through a
  `LandscapeDef*`, `m_nodes` through an `anim` pointer, a `LevelFile` built on
  the stack in `GlobalWorld.cpp`. *This batch's whole premise is that lesson
  applied one step earlier.*
- **`m_buildings` means four different things**, which is why
  `check_containers.py` skips the name. `Location`'s is a `FastSlotMap` whose
  indices are network identity. A name-based sweep would have converted it.
- **Converting a raw pointer to `unique_ptr` can introduce use-after-move.**
  Two were introduced and caught before commit — `zone->m_scrollZone` set after
  `push_back(zone)`, harmless before and undefined after. Scan every changed
  file for a local used after `std::move` of itself.
- **The stricmp hazard is not theoretical.** All 605 `ParentName` lines under
  `GameData/Shapes` say `sceneroot` in lower case while the code compares
  against `"SceneRoot"`. A `std::string` conversion reaching for `operator==`
  would have failed to parent every fragment in every shape in the game, with a
  green build and a green suite. `ShapeMarkerTests` pins it with a negative
  control.
- **Three defects were found and deliberately NOT fixed**, each given an owner
  instead: `ShapeMarker::m_depth` is used uninitialised when a marker block has
  no `Depth:` line (recorded on `strings/T20`); `LevelFile.cpp:275` bounds a
  transition mode against the wrong enum (now `language-hygiene/T13`). A third
  WAS fixed and stated as a fix: `TaskManager::RunTask` leaked its argument at
  capacity, which taking ownership in the signature ended.

---

## What I actually ran

Linux, at `50560ab`, on branch `claude/migration-batch-open-tasks-j02dys`,
which is level with `origin/main`.

All seven Python checks — all pass. `check_task_dag.py --next` on all five open
plans. The declared-list collision script below. Then, tree-wide greps to
measure real reach for the four batch candidates and for `ownership/T6`:
`g_globalWorld`, `g_globalWorld->m_*` by member, `INPUT_TYPE_`/`inputtype_t`,
`m_transitionMode`/`TRANSIT`, `NetDebugOut`/`RenderPointMarker`/`ReportError`,
the v-family, `SAFE_DELETE`/`SAFE_FREE`, `EmptyAndDelete`, and `\bNULL\b`.
Every declared `files` entry across the ten ready tasks was `ls`-checked; one
does not exist, and it is recorded above.

**Nothing here was built, and nothing was launched.** No MSVC and no Windows
client in this environment, so no claim is made about whether any of this
compiles. The Garden smoke test remains yours.

---

## What happened to Batches 1, 2, 3 and 4

Kept because the sequence is the evidence.

**Batch 1** — `directxmath/T19`, `ownership/T4`, `strings/T8`, three agents in
parallel. T19 landed on CI 513 and T4 on CI 514. `strings/T8` was measured,
found to be a different task than its file list implied, and released unstarted
with the finding written into its plan entry.

**Batch 2** was proposed as `strings/T12` then `strings/T11`, and executed as
`directxmath/T22` + `T23` instead: closing the owner gate made those ready, and
`strings/T12` collided with them over six `Species` files. **A batch proposal
expires the moment a gate closes.** Both math tasks landed, that plan finished
at 28 of 28, and it is archived.

**Batch 3** was `strings/T8` → `T12` → `language-hygiene/T10`, and executed as
`strings/T8` (CI 556) then `strings/T19` (CI 560) — T8 split in two on
measurement, as its own notes had proposed, though along a different line. It
found the four unowned variadic entry points that became `strings/T17` and
`T18`.

**Batch 4** was proposed as "close two plans" and closed neither, for the two
findings above. It did land six tasks, take `SAFE_FREE` and `EmptyAndDelete` to
zero tree-wide, retire the last `hygiene-ok` marker and add five tests.

**Batch 5** was proposed twice. The first proposal was never started and went
stale against nothing but a closer look; the second measured reach instead of
reading declared file lists, added a fourth task on the strength of that, and
executed all four. It is the first batch whose composition was decided by
measurement rather than by declaration — and its reach measurement was still
one file short, for a reason no member-name grep could have caught.

**The recurring lesson across all five is the same one:** a batch proposal is a
measurement with a short half-life, and every batch so far has found its own
premise partly wrong on contact. Batch 5 included. Measure before you claim,
not after CI does — and then expect the measurement to be wrong somewhere too.

---

## Where Batch 6 starts

Not a proposal — no collision measurement has been run since Batch 5 landed,
and this file's own history says not to trust one that has not. What is true
at the point Batch 5 finished:

**Ready for an agent:** `strings` T11, T12 and T17; `language-hygiene` T11;
`namespace` T2.

`ownership` closed on 2026-08-05 and stage 5 with it. Four pieces of raw
ownership outlived the plan's scope and **none has an owning task** —
`SAFE_DELETE_ARRAY`'s two callers, `GlobalEventCondition`'s two `char*`
members, `ColourShapeFragment`'s array allocation, and
`Resource::ListResources`' owning vector of owning `char*`. AGENTS.md lists
them. Stage 5 finishing is not a claim that raw ownership is extinct.

- **`strings/T17`** was Batch 5's named "obvious first task of Batch 6", and
  it still is — with one correction found while measuring: **its file list
  names `Tests/GameLogicTests/LevelFileRoundTripTests.cpp`, which does not
  exist under that or any name.** The tests its acceptance means are
  `Tests/NeuronClientTests/FileWriterTests.cpp` and
  `Tests/GameLogicTests/LevelFileTests.cpp`. Fix the path when you take it.
  Note it contests four files with the now-landed `ownership/T11`, so it is
  rebasing onto changed code rather than competing with it.
- **`language-hygiene/T11`** is newly unblocked by T10 and would close that
  plan. 473 sites, and it inherits `ControlTypes.inc` — whose first column it
  owns and whose second column T10 just rewrote. Expect the file to have moved.
- **`namespace/T2`** is still sequenced last on purpose and still declares
  whole directories.

**The gate has not moved.** `ownership/T6` and `determinism/T6` are both still
open, both still yours, and stage 5 still cannot end without the first.

---

## Reproducing the collision check

```python
# python3 - <tasks/_next-batch.md's script>  — run from the repo root
import yaml, glob, collections
ready = {  # refresh with: check_task_dag.py --next tasks/<plan>.yaml
    'determinism':           ['T6'],
    'language-hygiene':      ['T10', 'T13'],
    'namespace-migration':   ['T2'],
    'ownership':             ['T6', 'T11'],
    'strings-modernised':    ['T11', 'T12', 'T17', 'T18'],
}
owners = collections.defaultdict(list)
for path in glob.glob('tasks/*.yaml'):
    plan = path.split('/')[-1][:-5]
    if plan not in ready:
        continue
    for t in yaml.safe_load(open(path))['tasks']:
        if t['id'] in ready[plan]:
            for f in (t.get('files') or []):
                owners[f].append(f"{plan}/{t['id']}")
for f, who in sorted(owners.items()):
    if len(who) > 1:
        print(f, who)
```

**That script reads declared `files` lists, and it is wrong more often than it
is right.** Six have now been found wrong: `language-hygiene/T10` (11 declared
against 25, since corrected and now exact), `strings/T12` (2 against 45),
`determinism/T4` (a path that did not exist), `language-hygiene/T12` (a false
positive from a shared enumerator name), and — found for this batch —
`ownership/T11` (2 against 7) and `strings/T18` (5 against 9).
`check_task_dag.py` validates the graph, not whether a declared file is on disk
or whether the list is complete.

**Run it, then do not believe it.** `ls` a task's file list before starting it,
and measure its reach with a grep for what the tree actually spells — the
member name, the enumerator, the function name — not the type name and not the
task title. Every one of the six corrections above came from that grep, and
none of them came from the script.
