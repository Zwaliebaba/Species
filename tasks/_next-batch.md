# The next implementation batch

> # THERE IS NO NEXT BATCH. EVERY PLAN IS FINISHED.
>
> `strings-modernised` closed on 2026-08-05 at 20 of 20 and is in
> `tasks/Archive/` with the other ten. `tasks/` now holds `_template.yaml`, the
> three reading orders and the archive — no plan, no ready task, nothing to
> schedule. **This file is a record, not a proposal.** Read it before writing
> the next plan, for how its predictions turned out; do not read it to find
> work.
>
> What closed it, after the proposal below was overtaken:
>
> | Task | What it did |
> |---|---|
> | `strings/T13` | Nine Eclipse entry points and `SoundSystem::ParseSoundEvent` take `std::string_view`. 36 `.c_str()` calls went from 18 files — the ones T11 had just added. `EclRemoveWindow` turned out to READ ITS ARGUMENT AFTER DESTROYING THE WINDOW THAT OWNED IT, which it had done equally when the parameter was `char const*`. |
> | `strings/T17` | `FileWriter::printf` becomes the two-overload `std::string_view` + `std::format_string` shape, and the unbounded `char[10240]` goes. 127 format strings rewritten across 30 files. |
> | `strings/T9` | The tail: 44 code sites in 26 files, and `NewStr` deleted with its nine callers. Four latent defects fell out — a copy constructor that called `strlen(nullptr)`, a leak per sound blueprint, an unbounded write into a `char*[20]`, and an `strncpy` that left a 255-character filename unterminated. |
>
> **Three findings worth carrying into whatever gets planned next.**
>
> 1. **`%d` must become `{:d}`, never `{}`.** A bare `{}` writes a bool as
>    "true", a `char` as a character and a `float` as a float, where `%d` wrote
>    1, a number and garbage. T17 had four arguments that would have changed the
>    bytes of a level file under `{}` — three bools and an `unsigned char` — with
>    a green build and no test able to see it. Spelling the `d` also turns a `%d`
>    that was fed a float into a compile error.
> 2. **A grep keyed on `printf(` followed by ONE literal misses a concatenated
>    format.** `SoundSystem::WriteSoundEvent` spells its format as six adjacent
>    string literals over six lines. The sweep converted nothing there and
>    reported nothing. It was found by re-grepping for a surviving `%` inside a
>    brace-matched CALL rather than inside a literal — end any API sweep with
>    that second grep.
> 3. **An acceptance criterion that forbids naming what you removed gets the
>    documentation deleted.** T9's grep counts comments, and nine of the
>    survivors are prose explaining what an unbounded `vsprintf` used to do.
>    The criterion was amended to filter comment-only lines. That is the second
>    time this plan's closing grep has had to be corrected rather than met — the
>    first was `\bsprintf\b` not matching `vsprintf`.
>
> The original Batch 6 header follows.

> ## BATCH 6 WAS PROPOSED AND THEN NOT EXECUTED. The owner asked instead for
> the last tasks of `language-hygiene` and `namespace-migration`, and both
> plans are now closed and archived. What happened is at
> [**Progress — what was done instead**](#progress--what-was-done-instead).
> **The proposal below is kept as written**, because this file's premise is
> that a batch proposal is a measurement with a short half-life and the only
> way to see that is to leave the prediction next to the result. Its collision
> table is still the best measurement anyone has of the `strings` tasks; its
> plan-status counts are stale from the first table onward.

Written 2026-08-05 at `18d13ed`, level with `origin/main`. This is a proposal,
not a plan — the plans are the three YAML files beside it. It answers one
question: **of everything that is ready, what should the next batch be, and why
that rather than the rest.**

Read [`AGENTS.md`](../AGENTS.md) first. [`_restart.md`](_restart.md) is the
modernisation reading order; it is still current and this file does not
supersede it. **Batches 1 to 5 are done** — what they were and what they taught
is at the end of this file, and [**Progress — what Batch 5
did**](#progress--what-batch-5-did) is kept in full because Batch 6 is a direct
consequence of it.

> **The tree has never been in this shape before.** Every plan organised around
> migration stages 3 and 5 is closed. Nine tasks remain, they are the LARGEST
> remaining ones, and — this is the finding that shapes the batch — **they
> nearly all contest each other.** The comfortable four-agent batches are over.

---

## Where every plan stands

Counted from the YAML at `18d13ed`. **Eight plans are complete and in
`tasks/Archive/`; three are open with nine tasks between them.**

| Plan | done | todo | What is left |
|---|---:|---:|---|
| the eight archived plans | 104 | 0 | — (plus five `abandoned` in `layering-inversion`, superseded by its own T8–T18) |
| `language-hygiene` | 12 | 1 | **T11 only** — it closes the plan |
| `namespace-migration` | 2 | 3 | T2 → T4 → T5, in that order, nothing else |
| `strings-modernised` | 15 | 5 | the largest remaining plan, and the most contested |

All seven local checks pass at `18d13ed`.

**Nothing is gated on the owner.** `determinism/T6` and `ownership/T6` both
closed on 2026-08-05, and with them migration stage 5. Every one of the nine
open tasks is startable by an agent today — which has not been true before, and
is not the same as saying they can be started *at the same time*.

> `check_format` compares against `origin/main`. A container that cloned before
> the last merge reports a failure that is entirely stale refs — `git fetch
> origin main` first, then re-run.

### Every open task, and whether it can be started

Nine tasks. `--next` calls five of them ready, and none is owner work.

| Plan | Task | Ready? | Blocked on |
|---|---|---|---|
| `language-hygiene` | T11 — scope ControlType | **ready** | — |
| `namespace-migration` | T2 — NeuronClient into namespace Neuron | **ready** | — (sequenced last on purpose; see below) |
| `namespace-migration` | T4 — GameLogic into the game namespace | no | `namespace/T2` |
| `namespace-migration` | T5 — Species into the game namespace | no | `namespace/T4` |
| `strings-modernised` | T11 — the Eclipse widget name and caption members | **ready** | — |
| `strings-modernised` | T12 — the TextRenderer variadic text API | **ready** | — |
| `strings-modernised` | T17 — FileWriter's variadic printf | **ready** | — |
| `strings-modernised` | T13 — narrow char const* params to string_view | no | `strings/T11`, `strings/T12` |
| `strings-modernised` | T9 — sweep the long tail to zero | no | fifteen siblings; it is the plan's last node |

**`namespace/T5`'s last external blocker is gone.** It carried
`blocked_by: ownership/T6`, which closed on 2026-08-05. That chain is now
internal to its own plan for the first time.

---

## The finding that shapes this batch

**Of the five ready tasks, exactly ONE PAIR is disjoint.** Everything else
collides.

Batch 5's premise was that declared `files` lists under-report and reach has to
be measured. That was right, and it was applied again here — every one of the
five ready tasks was measured by grepping what the tree actually spells. The
result is not another set of file-list corrections, though there are two of
those. It is that **the remaining work is concentrated in the same files**,
which is what you would expect at the end of a modernisation: the easy,
isolated conversions went first.

Measured reach, and the contested-file count for every pair:

| | `lh/T11` | `s/T11` | `s/T12` | `s/T17` |
|---|---:|---:|---:|---:|
| **`lh/T11`** (38 files) | — | 9 | 12 | 3 |
| **`s/T11`** (30 files) | 9 | — | 12 | **0** |
| **`s/T12`** (46 files) | 12 | 12 | — | 14 |
| **`s/T17`** (31 files) | 3 | **0** | 14 | — |

`namespace/T2` is not in the table because it declares three whole
directories — `NeuronClient`, `GameLogic`, `Species` — and so contests all four
by construction.

### The two file lists that were wrong, and the one that was right

| Task | Declared | Measured | What happened |
|---|---:|---:|---|
| `strings/T11` | 8 | **30** | 23 missing, **and one of the eight holds none of the work** |
| `language-hygiene/T11` | 6 | **38** | list is the six declaring files; the count in its notes was 40% too high |
| `strings/T17` | 30 + 1 phantom | **31** | **exact** — first list in this tree to survive measurement |

**`strings/T11` is the worst list found here so far.** It declared eight files
against thirty, and `NeuronClient/WindowManager.cpp` was on it while containing
none of this work at all — its only matches are `m_titleHeight`, a different
member of a different class caught by a substring. It has been removed and the
23 real files added.

The under-count has a cause worth carrying: **`m_name` means twelve different
things in this tree.** Profiler, three SoundSystem classes, three Shape classes,
InputDriver, InputFilter, two LevelFile classes, GlobalWorld and two
TaskManagerInterface classes all declare one, and only EclButton's, EclWindow's
and ScrollBar's are this task. Grep the member name and you get 54 files and
over-report; grep the declaring class and you get 8 and under-report, because
every use site reaches the member through a pointer whose type is named nowhere
on the line. The thirty were arrived at by grepping the names and then **reading
every hit to classify its receiver**. There is no shortcut for a member-type
conversion, and this is the third batch in a row to learn it.

**`language-hygiene/T11`'s notes said 473 use sites in 64 files. It is 267
lines / 298 occurrences in 38 files**, plus `ControlTypes.inc`, which spells no
enumerator because it generates them by pasting `Control##x` and so appears in
no grep. The recorded figure came from a loose `\bControl[A-Z]\w*` sweep — 449
occurrences in 58 files, within rounding of it — of which **186 are not
enumerators**: `ControlTower` (45, a *building*), `ControlHelpSystem` (28),
`ControlMethod` (27), `ControlBindings` (25), `ControlHelpAccess` (14). The
prefix that names this enum's members is also the prefix of a building class and
a Species subsystem. Run off that number, a collision analysis puts
`GameLogic/ControlTower.*` and `Species/ControlHelp.*` in the task's reach; they
are not in it.

`language-hygiene/T12` was a false positive from a shared enumerator *name*.
This is the same error one level up, from a shared identifier *prefix*.
Regenerate the enumerator list from the `.inc` before believing any count.

**And `strings/T17`'s list was exactly right**, which is worth as much as the
two corrections. Grepping every `->printf(` and `.printf(` receiver tree-wide
returns precisely the 29 source files declared, plus `FileWriter.h/.cpp`.
Nothing missing, nothing idle. Only the test path was wrong — the
`LevelFileRoundTripTests.cpp` that does not exist, found while scoping Batch 5
and **now fixed in the YAML** rather than left as a note for whoever takes it.

Why that one held where the others did not, because it generalises:
**`FileWriter::printf` is reached through a function name on an object.**
`_out->printf(...)` spells the thing being converted at every call site. A
member-name grep misses a file that only calls `AddLocation`; an enumerator grep
misses a bare `return -1`; a function-name grep on the API being changed misses
nothing. **Expect a variadic-API conversion to measure honestly and a
member-type conversion not to.**

---

## The proposal

### Batch 6 — two agent tasks, measured disjoint, zero contested files

| | Task | Reach | Why it is in the batch |
|---|---|---:|---|
| 1 | `strings/T17` — FileWriter's variadic printf | 31 files | Batch 5 named it "the obvious first task of Batch 6" and the measurement agrees. 207 calls, the last unbounded write on the save path, and the only ready task whose file list is exact. |
| 2 | `strings/T11` — the Eclipse widget name and caption members | 30 files | The other half of the only disjoint pair. Newly measured at nearly four times its declared size, which is a reason to start it with eyes open rather than a reason to defer it. |

**Two, not four, and that is the honest number.** Batches 3, 4 and 5 ran three
to six tasks because the ready set was wide and shallow. It is now narrow and
deep: five ready tasks, ten pairs, and nine of the ten contest at least three
files. Padding this batch means picking a pair that collides and paying for it
in rebases or in a silently mismerged conversion.

**The pair is disjoint by construction, not by luck.** `T17` is the building
`Write()` methods, `LevelFile`, `GlobalWorld` and the sound blueprint writer —
the *save path*. `T11` is `EclButton`, `EclWindow`, `ScrollBar` and the UI
windows — the *screen*. They share no file, and the two GameLogic populations
barely overlap as code.

### The one hazard inside the batch, and it is not a collision

**`strings/T11` must add `.c_str()` at 17 lines in
`GameLogic/SpeciesWindow.cpp`, or ship undefined behaviour with a green build.**

Those 17 lines pass `EclButton::m_caption` — a raw `char*` today — straight into
`TextRenderer::DrawText2D` and `DrawText2DCentre`, which are still `char*, ...`
variadics until `strings/T12` converts them. Convert `m_caption` to
`std::string` and each of those becomes **a class type passed through `...`**,
which MSVC accepts with warning C4840 and which is undefined at runtime. Not a
compile error. Not something CI would fail on. It would print garbage or fault
at step 6 of the Garden smoke test, and the build that produced it would be
green.

This is recorded on both tasks' notes. `strings/T12`'s own measurement already
counted `m_caption` as 15 of the 116 sites needing its non-formatting
`string_view` overload — those are these sites, seen from the other end.
**If T12 were run first, T11's conversion would be free.** T12 contests 12 files
with T11 so they cannot be concurrent, and T12 contests 14 with T17 so it cannot
join this batch either. Hence: T11 adds the `.c_str()` calls now, and T12 removes
them later. Do not add a graph edge — they are not sequenced, they are merely
not concurrent.

### Not in the batch, and why

- **`language-hygiene/T11` — the near miss, and the one to run next.** It
  contests exactly three files with `T17`: `GlobalWorld.cpp` (5 sites),
  `Mine.cpp` (1) and `Rocket.cpp` (1). All seven are `controlEvent(ControlX)`
  calls and not one is a `printf`, so the collision is file-level rather than
  line-level — the smallest collision in the whole table. It is still excluded,
  on the same rule that kept `ownership/T11` away from `ownership/T6` over two
  lines in `App.cpp`, and that rule has not cost anything yet. **Start it the
  moment T17 lands; it closes `language-hygiene`.**
  - One thing its notes did not say and now do: `ControlBindings.h` declares
    `operator[]`, `getIcon`, `bind`, `setIcon` and `isAcceptibleInputType`
    **twice each** — once on `ControlType`, once on `controltype_t`, which is
    `typedef int`. Meeting the acceptance line "controltype_t names ControlType
    or is gone" makes each pair the same function. Decide which survives before
    converting, not after the redefinition errors arrive.
- **`strings/T12`.** 46 files and contests every other ready task. It is a batch
  on its own, and it should be the one after `lh/T11` — it unblocks `strings/T13`,
  and `T13` plus `T12` are two of the four things standing between the plan and
  its last node.
- **`namespace/T2`.** Still ready, still sequenced last on purpose, still
  declaring whole directories. **It is now the only plan with no cross-plan
  blocker left** — `namespace/T5`'s `ownership/T6` edge closed on 2026-08-05 —
  so `T2` → `T4` → `T5` is a clean three-task chain through `NeuronClient`,
  `GameLogic` and `Species` with nothing else in its way. It wants planning as a
  project and a quiet tree, not a batch slot.

### What the batch does not buy

**Neither task closes a plan.** `strings-modernised` goes from 5 open to 3, and
`language-hygiene` stays at 1 because its only task is the one excluded. Batch 4
also closed nothing and that was a finding; here it is arithmetic — no plan has
a single ready task left except `language-hygiene`, and that one collides.

**Neither task can be verified here.** Both have `msbuild` in their `verify`
list and both change the Garden smoke test's path — `T17` is the level loader's
writer half, `T11` is step 6, the Task Manager. An agent on Linux can run the
seven Python checks and read code. It cannot compile this and it cannot launch
it.

---

## The collision check

Five ready tasks across three plans. **On declared lists, ZERO contested files.
On measured reach, thirty-one.** That gap is the largest this file has recorded,
and it is the whole argument for the section above.

The declared-list script at the end of this file prints nothing at all for this
ready set — no pair shares a declared path. Believing it would put
`strings/T12`, `strings/T17` and `language-hygiene/T11` in one batch, three
tasks that contest 29 files between them.

Measured, by pair:

| Pair | Contested |
|---|---:|
| `strings/T12` × `strings/T17` | 14 |
| `strings/T12` × `strings/T11` | 12 |
| `strings/T12` × `language-hygiene/T11` | 12 |
| `strings/T11` × `language-hygiene/T11` | 9 |
| `strings/T17` × `language-hygiene/T11` | 3 |
| **`strings/T17` × `strings/T11`** | **0** |
| `namespace/T2` × anything | all of it, by construction |

**Do not read the zero as safety earned by luck.** It is the only zero, it was
found by measurement, and the two tasks still interact through
`SpeciesWindow.cpp`'s 17 `m_caption` lines — a hazard the file-level collision
table cannot express, because the collision is between one task's member and
another task's *signature*, in a file only one of them touches.

---

## What I actually ran

Linux, at `18d13ed`, on branch `claude/migration-batch-open-tasks-5nky7a`, which
is level with `origin/main`.

All seven Python checks — **all pass**. `check_task_dag.py --next` on all three
open plans. The declared-list collision script below, which printed nothing and
`ls`-checked every declared path across the five ready tasks — one does not
exist, and it is now fixed in the YAML rather than recorded as a caveat.

Then the reach measurements, tree-wide, for all five ready tasks: every
`->printf(`/`.printf(` receiver; `m_name`, `m_caption`, `m_title`,
`m_currentTextEdit` and `m_parentWindow` with each of the 54 hit files read to
classify its receiver; the 133 `ControlType` enumerators regenerated from
`ControlTypes.inc` and grepped exactly, then again loosely to reproduce the
recorded 473; every `DrawText2D`/`DrawText3D` family call; and the twenty-one
declarations of the contended member names. Pairwise intersections computed from
those lists, not from the declared ones.

**Nothing here was built, and nothing was launched.** No MSVC and no Windows
client in this environment, so no claim is made about whether any of this
compiles. Only YAML and Markdown changed in this commit — no C++ was touched.
The Garden smoke test remains yours.

---

## Progress — what was done instead

`language-hygiene` and `namespace-migration` are **closed and archived**. One
plan is open: `strings-modernised`, with five tasks.

| Plan | done | todo | What is left |
|---|---:|---:|---|
| the ten archived plans | 122 | 0 | — |
| `strings-modernised` | 17 | 3 | T13 and T17 ready; T9 is the plan's last node |

**`language-hygiene/T11` — scope ControlType.** 267 enumerator uses across 38
files, which is the corrected measurement rather than the 473-in-64 the plan
recorded. Landed in two commits, the first a whole-file reformat of the three
headers it rewrites. Three things in it were not mechanical: `getControlID`
returns `std::optional<ControlType>` rather than an invented enumerator,
because `ControlNull` is a real bindable index and the -1 it replaced was not;
five members of `ControlBindings` that were declared twice collapsed to one
each, and the bounds-checked body won; and the enum-to-table agreement is now
133 generated `static_assert`s instead of a comment asking you to check by eye.
Seven new tests over a name lookup that had none.

**`namespace-migration` T2, T4 and T5 — the whole rest of the plan.** 396
files wrapped: NeuronClient and its 45 cross-project forward declarations into
`namespace Neuron`, then GameLogic and Species into `namespace Species`.

Four things worth carrying out of it:

- **A using-directive makes a name findable; it does not fix a forward
  declaration.** `NeuronCore.h` ends with `using namespace Neuron;`, which is
  why T2 changed no caller at all — but `class Renderer;` in a GameLogic header
  still declares a new `::Renderer`, and that is a link error pointing
  somewhere else entirely. 51 forward declarations across the tree are wrapped
  now. `check_layering.py` sees none of this, because a forward declaration
  includes nothing.
- **T4 and T5 were not separable**, and the plan could not have known it: T1
  chose ONE namespace for all game code, so wrapping GameLogic alone leaves
  every unqualified GameLogic name in `Species/` unresolvable. The reverse
  order would have had buildable intermediates.
- **The namespace broke a check, and the fix was a narrowing.**
  `check_math_types.py` found a function body by looking for a brace at depth
  zero, and `namespace Neuron {` is one — so a wrapped file read as a single
  body holding every function in it. Braces are classified as they open now.
- **Being forced to classify every forward declaration found dead ones.**
  `struct IDirect3DVertexBuffer9` is declared in two GameLogic headers and used
  nowhere; `GlobalInternet.cpp` has an unclosed brace inside an `#ifdef DEBUG`
  that is defined nowhere. Both left alone, both recorded.

**None of it was compiled.** No MSVC on Linux, and for a namespace migration
that is a weaker guarantee than usual: these fail where a name resolves
differently, and only a compiler resolves names. The seven Python checks pass
and every structural invariant was verified by script over the whole tree —
brace balance, no `#include` inside a wrapper, no name clash across the
namespace boundary, no global whose declaration and definition straddle it.

### And then T12 and T11 landed too

`strings-modernised` is at **17 of 20**. The owner asked for the plan finished
rather than for a batch, so T12 went first — the two-overload split it needs
makes T11's `m_caption` sites free — and T11 followed.

**`strings/T12`** replaced ten variadic `DrawText` entry points with a plain
`std::string_view` overload and a `std::format` template each. The template
takes AT LEAST ONE ARGUMENT, which is what keeps the pair unambiguous; 147 of
the 199 call sites pass only text. 92 format strings were rewritten. Nothing
changed output: no zero-argument literal contained a `%`, and no site used a
bare `%f`, which is the one spec whose default differs between printf and
std::format.

**`strings/T11`** made the Eclipse widget names and captions `std::string`.
The predicted case-sensitivity trap was real and it was seventeen sites — the UI
matches buttons on their captions with `stricmp` while Eclipse's own lookup
uses `strcmp`, so a blanket sweep to `==` would have broken the first and left
the second correct. They call `Neuron::StrEqualsIgnoreCase` now. Four defects
fell out on the way, including `EclButton::SetProperties` silently REFUSING a
name longer than 256 — the button kept the name "New Button" and every lookup
by the intended name missed.

**What is left: T13, T17 and T9.** T13 narrows five Eclipse functions and
`SoundSystem::ParseSoundEvent` to `string_view` and deletes the 30 `.c_str()`
calls T11 just added — it is the natural next task and its scope is now exactly
known. T17 is FileWriter's variadic printf, still the largest single piece.
T9 is the plan's last node and depends on both.

**Re-measure before starting any of them.** Every collision number below was
taken before 396 files moved into namespaces and two of the five tasks landed.

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

## What Batch 5 ran, kept for comparison

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

## What happened to Batches 1, 2, 3, 4 and 5

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

## Where Batch 7 starts

Not a proposal — the measurement above has a short half-life and this file's own
history says not to trust one taken before the previous batch landed. What is
true at the point Batch 6 was proposed:

**Ready for an agent, in the order they should be taken:**

1. **`language-hygiene/T11`** — the moment `strings/T17` lands. Three contested
   files with it, seven lines, all `controlEvent` calls. **It closes
   `language-hygiene`**, which would make it the ninth plan archived.
2. **`strings/T12`** — a batch on its own at 46 files. It unblocks
   `strings/T13`, and it retroactively removes the 17 `.c_str()` calls
   `strings/T11` has to add in this batch.
3. **`namespace/T2` → `T4` → `T5`** — the last plan standing, now with **no
   cross-plan blocker anywhere in it**. `namespace/T5` carried
   `blocked_by: ownership/T6` until 2026-08-05; that closed, and the chain is
   internal for the first time. It wants planning as a project.

After `T12`, `strings-modernised` has only `T13` and `T9` left, and `T9` is its
last node by construction — it depends on fifteen siblings.

**Raw ownership did not go extinct when stage 5 closed**, and none of the four
survivors has an owning task: `SAFE_DELETE_ARRAY`'s two callers,
`GlobalEventCondition`'s two `char*` members, `ColourShapeFragment`'s array
allocation, and `Resource::ListResources`' owning vector of owning `char*`.
`AGENTS.md` lists them. **A fifth is now measured and it does have an owner:**
`EclButton::m_caption` and `m_tooltip` are `new char[]`/`delete[]` with a
`strcpy` each, and `strings/T11` retires them along with the `char[N]` members.

**Nothing is gated on the owner for the first time in this file's history.** The
two things that would still be worth a Garden run, neither of them blocking:
`ownership/T6`'s four commits reached the main menu only on CI, never in front
of a game; and three of `determinism/T5`'s six fixed RNG sites — Spam, GodDish
and Library — are not in The Garden and remain unexercised.

---

## Reproducing the collision check

```python
# python3 - <tasks/_next-batch.md's script>  — run from the repo root
import yaml, glob, collections, os
ready = {  # refresh with: check_task_dag.py --next tasks/<plan>.yaml
    'language-hygiene':      ['T11'],
    'namespace-migration':   ['T2'],
    'strings-modernised':    ['T11', 'T12', 'T17'],
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
                if not os.path.exists(f):          # the ls check, inline
                    print('MISSING ON DISK:', f, plan, t['id'])
for f, who in sorted(owners.items()):
    if len(who) > 1:
        print(f, who)
```

**At `18d13ed` that script prints NOTHING**, and it is the most misleading
output it has ever produced. Zero declared collisions across the five ready
tasks; thirty-one measured ones. Believing it puts `strings/T12`, `strings/T17`
and `language-hygiene/T11` in one batch — three tasks contesting 29 files.

**Eight declared lists have now been found wrong.** `language-hygiene/T10` (11
declared against 25, since corrected and now exact), `strings/T12` (2 against
45), `determinism/T4` (a path that did not exist), `language-hygiene/T12` (a
false positive from a shared enumerator name), `ownership/T11` (2 against 7),
`strings/T18` (5 against 9), and — found for Batch 6 — **`strings/T11` (8
against 30, one of the eight holding none of the work)** and
**`language-hygiene/T11`** (6 declaring files, 38 real, and a site count 40% too
high from a loose prefix grep). Both are corrected in the YAML.

**One has been found right**, and it is the useful control: `strings/T17`, whose
30 source paths were exact. The difference is what the conversion is reached
*through* — a function name on an object appears at every call site, a member
name or an enumerator does not.

**Run it, then do not believe it.** `ls` a task's file list before starting it —
the script now does that inline — and measure its reach with a grep for what the
tree actually spells: the member name, the enumerator regenerated from its
generator, the function name. Not the type name, not the identifier prefix, and
not the task title. Every one of the eight corrections came from that grep, and
none came from the script.
