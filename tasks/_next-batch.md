# The next implementation batch

Written 2026-08-05 at `e7a1a88`, the merge that closed
`directxmath-migration` and archived it. This is a proposal, not a plan — the
plans are the five YAML files beside it. It answers one question: **of
everything that is ready, what should the next batch be, and why that rather
than the rest.**

Read [`AGENTS.md`](../AGENTS.md) first. [`_restart.md`](_restart.md) is the
modernisation reading order; it is still current and this file does not
supersede it. **Batches 1 and 2 are done** — what they were and what they
taught is at the end of this file.

Four plan files changed with this proposal, and none of the changes re-scopes
an existing task. `strings-modernised` gained T17 and T18, which own work its
own acceptance greps could not see; two file lists were corrected from
measurement; `ownership/T8` records the decision that unblocked it; and
`determinism.yaml` came back out of `Archive/` with two scoping tasks. All four
are described below, and none of the new work is in this batch.

---

## Where every plan stands

Counted from the YAML at `e7a1a88`. Six plans are complete and in
`tasks/Archive/`; five are open.

| Plan | done | todo | State |
|---|---:|---:|---|
| `directxmath-migration` | 28 | 0 | complete — **archived since Batch 2** |
| the five earlier plans | 59 | 0 | complete — archived |
| `strings-modernised` | 11 | 7 | T8, T11, T12 ready; T17 and T18 are new |
| `ownership` | 5 | 4 | T8 ready and now unblocked; T5 waits on `strings/T8` alone |
| `language-hygiene` | 9 | 3 | T10 and T12 ready |
| `namespace-migration` | 2 | 3 | T2 ready, sequenced last by design |
| `determinism` | 2 | 2 | **reopened 2026-08-05** — T3 and T4 are scoping tasks |

Nineteen tasks open. Ten are offered by `--next` — the seven that were ready
before this proposal, plus `strings/T18` and the two reopened determinism
tasks.

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
| `ownership/T8` | 38 `m_sounds` uses, 26 `ShutdownSound`/`InitialiseSound` sites. **The decision it waited on is taken**, so this is now a size rather than a blocker. | T8 `notes` |
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

### Batch 3 — three tasks in sequence, and three that may run beside them

**This batch cannot fan out, and that is the second finding.** Of the ten
ready tasks only five are `parallel_safe: true`, and three of those five — the
new `strings/T18` and the two reopened `determinism` scoping tasks — touch
nothing anybody else claims and can run beside the batch. The other two are the
batch's own `strings/T8` and `language-hygiene/T12`, and those two collide over
six files:
`GlobalWorld.cpp`, `LevelFile.cpp`, `Location.cpp`, `GameCursor.cpp`,
`Main.cpp` and `Script.cpp`. Every other ready task is exclusive by its own
flag. Batch 1 ran three agents at once because its three tasks happened to be
disjoint; that is not available here, and pretending otherwise buys a merge
conflict in the level-file writers.

| | Task | Why it is in the batch |
|---|---|---|
| 1 | ~~`strings/T8`~~ — **done, CI 556** | **The only thing standing between today and the end of stage 5.** `ownership/T5` is blocked by this task and nothing else; T5 → T6 (App owns its subsystems) → T7 (delete the macros) is then everything `ownership` has left except T8, and `namespace/T5` is behind it too. It also gates `strings/T9` and the new T17. Four tasks and the end of stage 5, behind one node. |
| 2 | `strings/T12` — the TextRenderer variadic API | The largest single safety win available, and the measurement below changes what it is. `parallel_safe: false`; run it alone. |
| 3 | `language-hygiene/T10` — InputType, and delete the dead `ControlTypes.cpp` | **File-disjoint from every other ready task**, measured, so it is the safest thing to run whenever the tree is free. It unblocks `language-hygiene/T11` (473 sites), the last node in that plan. |

**`strings/T8` was split before it was started, as its own notes proposed —
and the line moved.** The notes proposed "the colour filenames plus the
locals"; measuring the members first put it **by class** instead, because
`m_mapFilename` and `m_missionFilename` are each declared TWICE — on
`LevelFile` and on `GlobalLocation` — and the recorded "28 uses in 9 files"
was both classes added together. `strings/T8` is now LevelFile's five members
and the local buffers in all three files; **`strings/T19`** is
`GlobalLocation`'s three plus `GlobalEventAction::m_filename`, ten files, and
the width-padded location row.

One correction to the argument above, and it matters: **the split does not
unblock `ownership/T5` on its own.** T5 names `GlobalWorld.cpp`, so it needs
both halves, and its `blocked_by` now says so. What the split buys is two
reviewable diffs and a byte-identity test that exists before the risky half
starts — not an earlier start for stage 5.

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
- **`ownership/T8` (SoundInstance).** **Unblocked as of 2026-08-05** — the
  decision it waited on was put to the owner and answered; see *The decisions*
  below. It is not in the batch because the batch was already three deep and
  this one wants the tree to itself, but it is now schedulable work rather than
  a question, and it is the obvious first task of Batch 4.
- **`determinism/T3` and `T4`.** New, `parallel_safe: true`, and disjoint from
  everything — but they are reading tasks whose deliverable is an answer, not a
  diff, so they do not compete with the batch for a slot. T3 wants to be read by
  whoever takes `ownership/T8`, in a separate commit.
- **`strings/T17` and `T18`.** New, and T17 depends on T8, so neither belongs
  here. T18 is small, self-contained and `parallel_safe: true`, and its five files
  are claimed by nothing else — so unlike everything else here it CAN run
  beside the batch rather than after it. It is the one slot a second agent
  could take.
- **`namespace/T2`.** Ready, sequenced last on purpose. Every file in this
  batch is a file it would have to move.

### The decisions, put to the owner and taken 2026-08-05

Four questions were open when this proposal was written. All four were asked
and all four are answered; each is recorded on the task it governs, not only
here, because this file is a proposal and proposals expire.

**1. Who owns a `SoundInstance` between `new` and `InitialiseSound` succeeding
— `ownership/T8`'s blocker.** *Decided: `InitialiseSound` takes
`std::unique_ptr<SoundInstance>`.* It stores the instance on success and
destroys it on the monophonic folding path, `m_sounds` becomes a
`SlotMap<unique_ptr>`, and `ShutdownSound` serves registered instances only —
one contract, in the header. The cheaper option, keeping the raw parameter and
splitting the two cases inside `ShutdownSound`, was refused for leaving the
transfer in a comment: `ownership` T2 and T9 exist to stop doing exactly that.
**T8 is no longer blocked on anything but an agent.**

**2. `ShutdownSound`'s missing identity check.** *Decided: add it, as a fix and
labelled as one.* The slot is released only when it still holds this instance.
Today's `ValidIndex`-only test can un-register a live sound belonging to
someone else after an index is reused, and sound destructors stop audio
channels, so the failure is silent. Preserving today's behaviour exactly was
offered and refused. It lands in T8's commit and says in the message that it is
a behaviour change rather than part of the conversion.

**3. Where the two unowned determinism findings live.** *Decided: reopen
`determinism.yaml`.* It has moved out of `Archive/` and carries two new SCOPING
tasks — **T3**, `SoundInstance.cpp`'s two `speciesRandom()` draws on
client-configuration-dependent paths, and **T4**,
`LandscapeRenderer::GetLandscapeColour` reseeding the simulation RNG from
rendering code. Neither may change behaviour; each ends with an answer written
into the plan, and whether it becomes a fix is a later decision. Both are
reading tasks that work on Linux, both are `parallel_safe: true`, and the
fourteen paths that named the archived file are updated.

That reopening also retires a circular sentence. `AGENTS.md` said the second
finding "belongs in `determinism.yaml` once somebody establishes what it
costs" — but establishing that *is* the work, which is why it sat unowned
across three batches. The plan's own scope note had the same shape and is
rewritten: a fix needs to be understood before it is planned, an investigation
does not.

**4. The batch order.** *Decided: as proposed* — `strings/T8`, then
`strings/T12`, then `language-hygiene/T10`, with `strings/T18` available beside
them. Starting with T12 (the live undefined behaviour) or with T10 (the lowest
risk) were both offered; T8 won on the same argument the table above makes,
that it is the only node between here and the end of stage 5.

**`ownership/T8` and `determinism/T3` are the same file and must not be the
same commit.** Whoever opens `SoundInstance.cpp` for the ownership conversion
is in the right place to scope the RNG draws, and a lifetime conversion plus an
RNG question in one diff is precisely the review nobody can judge. Both tasks
now say so.

---

## The collision check

`--next` offers ten tasks across five plans and says nothing about whether
they can run together, because it reasons per plan. Cross-referencing the
**measured** reach of each — not the declared `files` lists, which are
under-declared in two of the ten — finds this:

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

## Progress

**`strings/T8` landed on CI 556 (`6b38509`), 2026-08-05.** Eight files, 33 call
sites, eleven new tests, x64 Debug green and 180 tests passing. It was split
first — see the plan entry — and the split produced `strings/T19`, which is now
the batch's first item instead. `ownership/T5` waits on T19 alone.

Two things came out of it that the rest of the batch should carry:

- **`FileWriter::printf` is pinned now**, eight tests over the formats the
  writers actually use. `strings/T19` and `T17` both have to prove they change
  no bytes, and this is what they prove it against.
- **`AGENTS.md`'s test count was wrong.** It said 180; CI counted **169** at
  `e7a1a88`, and T8's eleven made the stale figure accidentally true. Corrected
  there, with how to read the real number. It cost ten minutes of believing the
  new tests had not run.

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

The four decisions above were **put to the owner and answered on 2026-08-05**,
not inferred from the code. Each is recorded on the task it governs as well as
here — `ownership/T8`'s notes and acceptance, `determinism.yaml`'s summary and
its two new tasks, and `AGENTS.md`'s two Known-issues bullets, which now name
an owning task instead of describing an orphan. `check_task_dag.py` passes on
all five open plans.

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
