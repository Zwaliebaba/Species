# The next implementation batch

Written 2026-08-05 at `e7a1a88`, the merge that closed
`directxmath-migration` and archived it. This is a proposal, not a plan — the
plans are the four YAML files beside it. It answers one question: **of
everything that is ready, what should the next batch be, and why that rather
than the rest.**

Read [`AGENTS.md`](../AGENTS.md) first. [`_restart.md`](_restart.md) is the
modernisation reading order; it is still current and this file does not
supersede it. **Batches 1 and 2 are done** — what they were and what they
taught is at the end of this file.

Two plan files changed with this proposal, and both changes are additions
rather than re-scopes: `strings-modernised` gained T17 and T18, which own work
its own acceptance greps could not see, and two file lists were corrected from
measurement. Both are described below and neither is in this batch.

---

## Where every plan stands

Counted from the YAML at `e7a1a88`. Seven plans are complete and in
`tasks/Archive/`; four are open.

| Plan | done | todo | State |
|---|---:|---:|---|
| `directxmath-migration` | 28 | 0 | complete — **archived since Batch 2** |
| `determinism` | 2 | 0 | complete — archived |
| the five earlier plans | 59 | 0 | complete — archived |
| `strings-modernised` | 11 | 7 | T8, T11, T12 ready; T17 and T18 are new |
| `ownership` | 5 | 4 | T8 ready; T5 waits on `strings/T8` alone |
| `language-hygiene` | 9 | 3 | T10 and T12 ready |
| `namespace-migration` | 2 | 3 | T2 ready, sequenced last by design |

Seventeen tasks open. Eight are offered by `--next` — the seven that were
ready before this proposal, plus the new `strings/T18`.

---

## What is ready, measured

Every number was produced on Linux at `e7a1a88` by the command in the last
column. None of it was built.

| Task | Size | Command |
|---|---|---|
| `strings/T8` | **43** strcpy-family calls in 3 files — but the work is 7 members across ~10 files, and **113 `FileWriter::printf` calls** it does not own. See below. | `grep -Eow 'sprintf\|…'` |
| `strings/T12` | **225** call sites in 44 files, and they are **two populations**: 109 literal formats, **116 runtime format strings**. | see T12 `notes` |
| `strings/T11` | **6** fixed `char[N]` members over 3 headers; 30 C-string calls in the 8 declared files. | `grep -nE 'char\s+m_\w+\s*\['` |
| `language-hygiene/T10` | **218** `INPUT_TYPE_` sites — in **25** files, not the 11 the task declared. | `grep -rEo '\bINPUT_TYPE_\w+'` |
| `language-hygiene/T12` | **22** files carry a `Camera::Mode` enumerator, not the 21 declared. | `grep -rEow 'Mode[A-Z]\w+'` |
| `ownership/T8` | 38 `m_sounds` uses, 26 `ShutdownSound`/`InitialiseSound` sites — behind **a decision**. | T8 `notes` |
| `namespace/T2` | whole of `NeuronClient`. Deliberately last. | — |

All seven local checks pass at `e7a1a88`:

```
check_project_files  check_layering  check_task_dag  check_containers
check_math_types     check_format    check_hygiene            — all OK
```

> `check_format` compares against `origin/main`. A container that cloned before
> the last merge reports a failure that is entirely stale refs — `git fetch
> origin main` first, then re-run. It cost twenty minutes here; it is written
> down so it costs nobody else any.

---

## The finding that shapes this batch

**There are four variadic `char const*, ...` entry points left in the tree, all
four `vsprintf` into a fixed stack buffer, and only one of them had a task.**

| Entry point | Call sites | Owner |
|---|---:|---|
| `TextRenderer`'s ten `DrawText*` | 225 | `strings/T12` |
| **`FileWriter::printf`** | **207** | **nobody — now `strings/T17`** |
| `NetLib::NetDebugOut` | 16 | nobody — now `strings/T18` |
| `DebugRender::RenderPointMarker` | 3 | nobody — now `strings/T18` |
| `Script::ReportError` | 1, its own declaration | nobody — now `strings/T18` |

They were invisible because every acceptance grep in `strings-modernised` names
the strcpy family, and `\bsprintf\b` does not match `vsprintf` — the `v` is a
word character. **`strings/T9`, the task that ends stage 4 by declaring a
tree-wide zero, could have passed with eleven unbounded `vsprintf` calls
still in the tree.** Its grep now names the v-family and it depends on T17 and
T18.

`FileWriter::printf` is the one that matters, and it changes how to read
`strings/T8`. It is the API every level, location and profile file is written
through — `_out->printf("\t%4d %4d %30s %40s\n", …)` — and it `vsprintf`s into
a `char[10240]` with no bound, then runs the encryption loop over the result.
113 of its 207 call sites are in `LevelFile.cpp` and `GlobalWorld.cpp`, the two
files `strings/T8` owns.

So the width-specified location-table write that `strings/T8`'s notes flag as
its sharp edge is **not reachable from T8's own conversion**: it is a
`FileWriter::printf` call, and T8's acceptance grep does not match it. T8 can
meet every criterion it has without touching a byte of file-output formatting.
That is not a defect in T8 — it is the boundary between building a buffer and
writing one — but anyone reading T8 as "the task that makes the file writers
safe" would stop half way and not know it.

---

## The proposal

### Batch 3 — three tasks in sequence, and one that may run beside them

**This batch cannot fan out, and that is the second finding.** Of the eight
ready tasks only three are `parallel_safe: true`, and one of those three is the
new `strings/T18` — five files, none of them anybody else's. The other two are
`strings/T8` and `language-hygiene/T12`, and they collide over six files:
`GlobalWorld.cpp`, `LevelFile.cpp`, `Location.cpp`, `GameCursor.cpp`,
`Main.cpp` and `Script.cpp`. Every other ready task is exclusive by its own
flag. Batch 1 ran three agents at once because its three tasks happened to be
disjoint; that is not available here, and pretending otherwise buys a merge
conflict in the level-file writers.

| | Task | Why it is in the batch |
|---|---|---|
| 1 | `strings/T8` — the level and profile writers | **The only thing standing between today and the end of stage 5.** `ownership/T5` is blocked by this task and nothing else; T5 → T6 (App owns its subsystems) → T7 (delete the macros) is then everything `ownership` has left except T8, and `namespace/T5` is behind it too. It also gates `strings/T9` and the new T17. Four tasks and the end of stage 5, behind one node. |
| 2 | `strings/T12` — the TextRenderer variadic API | The largest single safety win available, and the measurement below changes what it is. `parallel_safe: false`; run it alone. |
| 3 | `language-hygiene/T10` — InputType, and delete the dead `ControlTypes.cpp` | **File-disjoint from every other ready task**, measured, so it is the safest thing to run whenever the tree is free. It unblocks `language-hygiene/T11` (473 sites), the last node in that plan. |

**`strings/T8` should be split before it is started, exactly as its own notes
propose.** They already lay out the line: the three `LevelFile` colour
filenames plus the local buffers in `LevelFile.cpp` and `Script.cpp` are
containable in the declared three files, while `GlobalLocation`'s three members
and `GlobalEventAction::m_filename` are one coupled change over ten files —
`m_mapFilename` is 28 uses in 9 files and `m_missionFilename` 32 in 10, both
re-measured here and both unchanged since. That split is what `strings/T5`'s
re-scope did, it is what made T5 land after being discarded once, and the
containable half is what `ownership/T5` actually needs.

**Write the round-trip test in the containable half regardless of how it
splits.** `layering-inversion/T15` moved `LevelFile` and `GlobalWorld` into
`GameLogic` and emptied `LinkStubs.cpp`, so a level-file round trip is a real
`GameLogicTests` test now rather than a review argument. `strings/T17` is
written to depend on that test existing — it is the only way to convert a
`%30s` field width and know the profile still writes the same bytes.

**`strings/T12` is two conversions wearing one number, and the split is not the
one the task describes.** Measured at `e7a1a88`, over the 225 call sites
outside `TextRenderer.cpp`:

- **109 pass a string literal as the format**, 89 of them with arguments after
  it. These are the `std::format` conversions the task was written for, and
  they are where a wrong argument type becomes a compile error instead of
  runtime undefined behaviour. `strings/T4` already converted
  `PersistingDebugRender.h` to exactly this shape — four
  `std::format_string<Args...>` templates — so the pattern is in the tree and
  does not need inventing.
- **116, in 22 files, pass no literal at all.** The format string is a runtime
  value: `m_caption` (15 sites), `LANGUAGEPHRASE(…)`, `caption.c_str()`,
  `loc->m_mapFilename`, `islandName`, `InputField::Render`'s `m_buf.c_str()`.
  None of them passes an argument after it.

`std::format_string` is `consteval`, so a runtime `char const*` does not
convert to it and **all 116 break under a single format template**. They want a
second, non-formatting overload taking `std::string_view` — and converting them
that way is a fix, not a translation, because today every one of them hands
user data, level-file data or a translated phrase to `vsprintf` *as a format
string*.

How live that is, stated rather than implied: the shipped language files
contain exactly one `%` — `Russian.txt:122`, a key no code references — so the
hazard is latent in shipped content and unbounded in anything a user or a level
file supplies. That is worth writing in the commit as what the task fixed;
"225 call sites converted" undersells it.

### Not in the batch, and why

- **`strings/T11` (the Eclipse widget names).** Ready and genuinely worth
  doing — it is half of `strings/T13`'s dependency, and Batch 2's finding still
  holds: there is **no `stricmp` on a widget name anywhere**, so its fourth
  acceptance line is satisfied by a straight `operator==`. It is fourth in the
  queue rather than in the batch because `m_name` is the tree's most contended
  member name and its real blast radius cannot be measured by grep — the same
  reason `check_containers.py` skips the name rather than guessing. Give it a
  clear tree and a reader, not a slot beside two other conversions.
- **`language-hygiene/T12` (`Camera::Mode`).** Ready and `parallel_safe: true`,
  and it collides with `strings/T8` over six files. Same conclusion as Batch 1
  reached for the same reason, re-measured: it is the one to hold back.
- **`ownership/T8` (SoundInstance).** Still not schedulable, and still for the
  same reason — its blocker is a decision, not work. **Ask for it with this
  batch** (see below).
- **`strings/T17` and `T18`.** New, and T17 depends on T8, so neither belongs
  here. T18 is small, self-contained and `parallel_safe: true`, and its five files
  are claimed by nothing else — so unlike everything else here it CAN run
  beside the batch rather than after it. It is the one slot a second agent
  could take.
- **`namespace/T2`.** Ready, sequenced last on purpose. Every file in this
  batch is a file it would have to move.

### One decision to ask the owner for, worth asking now

`ownership/T8` cannot start until somebody decides who owns a `SoundInstance`
between `new` and `InitialiseSound` succeeding: `ShutdownSound` serves both
registered and unregistered instances, deletes unconditionally, and tests
`ValidIndex(m_id.m_index)` without checking the slot still holds *this*
instance. Adding that check is a behaviour change and probably a fix.

Ask for it in the same message that claims this batch, because **the same file
holds the tree's oldest unfixed determinism finding** — `SoundInstance.cpp`
draws twice from `speciesRandom()` on paths whose execution depends on
client-local sound configuration, which is a desync no build or test would show
(`AGENTS.md`, *Known issues*). Whoever opens that file for T8 is in the right
place to scope it, and scoping it is a reading task that works on Linux.

Neither that finding nor `LandscapeRenderer::GetLandscapeColour` reseeding the
simulation RNG from rendering code has an owning task, and `determinism.yaml`
is now archived with both of its tasks done. Giving them a home means bringing
that plan back out of `Archive/`, which is a structural change and the owner's
call rather than a batch's — flagged here, not done.

---

## The collision check

`--next` offers eight tasks across four plans and says nothing about whether
they can run together, because it reasons per plan. Cross-referencing the
**measured** reach of each — not the declared `files` lists, which are
under-declared in two of the eight — finds this:

| Pair | Contested files |
|---|---|
| `strings/T8` × `lh/T12` | 6 — GlobalWorld.cpp, LevelFile.cpp, Location.cpp, GameCursor.cpp, Main.cpp, Script.cpp |
| `strings/T11` × `lh/T12` | 7 (partly `m_name` contention, see below) |
| `strings/T11` × `strings/T12` | 5 |
| `strings/T12` × `lh/T12` | 5 |
| `strings/T8` × `strings/T11` | 2 |
| `lh/T10` × everything ready | **0** |

Two cautions on reading that table. `m_name` means a widget name, a location
name, a sound-group name and a profiler element name in four different places,
so a name-based measurement of `strings/T11` over-reports — three of its seven
apparent collisions with `lh/T12` are a different `m_name`. And
`namespace/T2` declares whole directories, so it contests everything by
construction; that is what "sequenced last" means in practice.

Reproduce with the script at the end of this file.

---

## What I actually ran

Linux, at `e7a1a88`. All seven Python checks — all pass, after
`git fetch origin main`. `check_task_dag --next` on every plan. The greps and
call-site scans quoted above, including a brace-matching scan of all 225
`DrawText*` call sites to classify literal against runtime format strings.
`clang-format 18.1.3` against fifteen candidate files: `GlobalWorld.cpp`,
`LevelFile.cpp`, `TextRenderer.h/.cpp`, `EclWindow.h/.cpp` and
`SoundSystem.h/.cpp` are already clean; `Script.cpp` (181 lines),
`ScrollBar.h/.cpp` (68) and `WindowManager.cpp` (23) are not, so **`strings/T8`
and `strings/T11` each need a whole-file reformat commit before their
conversion** and `strings/T12` does not.

**Nothing here was built, and nothing was launched.** No MSVC and no Windows
client in this environment, so every size is a count of lines and call sites
rather than a compile, and the Garden smoke test remains the owner's. This
batch is a scheduling argument; it is not evidence that any of it compiles.

---

## What happened to Batches 1 and 2

Kept because the sequence is the evidence, and because Batch 2 was not what
Batch 1's proposal predicted.

**Batch 1** — `directxmath/T19`, `ownership/T4`, `strings/T8`, three agents in
parallel. T19 landed on CI 513 and T4 on CI 514. `strings/T8` was measured,
found to be a different task than its file list implied, and **released
unstarted with the finding written into its plan entry** — which is why it is
first in this batch rather than done. The owner gate ran once and closed
`directxmath/T21` and `determinism/T2` together, exactly as Batch 1 argued it
should.

**Batch 2** was proposed as `strings/T12` then `strings/T11`, and was executed
as `directxmath/T22` + `T23` instead. Closing the owner gate made T22 and T23
ready, and `strings/T12` — two declared files, 44 real ones — collided with
them over six `Species` files. **The lesson is the one this file's collision
table exists for: a batch proposal expires the moment a gate closes.** Both
math tasks landed, the plan finished at 28 of 28, and it is archived.

The proposal that ran those two batches is superseded by this file. Its three
housekeeping items are settled: `AGENTS.md` no longer points at
`layering-inversion/T11` for tests nobody wrote, the `AsLegacy` worklist is
gone with the seam, and the two determinism findings are still unowned — see
*One decision to ask the owner for*, above.

---

## Reproducing the collision check

```python
# python3 - <tasks/_next-batch.md's script>  — run from the repo root
import yaml, glob, collections
ready = {  # refresh with: check_task_dag.py --next tasks/<plan>.yaml
    'language-hygiene':      ['T10', 'T12'],
    'namespace-migration':   ['T2'],
    'ownership':             ['T8'],
    'strings-modernised':    ['T8', 'T11', 'T12', 'T18'],
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

**That script reads declared `files` lists, and this batch found two that were
wrong** — `language-hygiene/T10` declared 11 files against a reach of 25, and
`strings/T12` declares 2 against 45. Both are corrected or recorded in their
plan entries now. When a task's list looks thin, measure its reach with a grep
for what the tree actually spells — the enumerators, the call, the member —
rather than for the type name, and correct the list before trusting this
script.
