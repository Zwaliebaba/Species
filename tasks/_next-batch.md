# The next implementation batch

Written 2026-08-05 at `97b5844`, the merge that closed Batch 3. This is a
proposal, not a plan — the plans are the five YAML files beside it. It answers
one question, and this time a narrower one than usual: **which open plans can
actually be FINISHED, and what stands between here and finishing them.**

Read [`AGENTS.md`](../AGENTS.md) first. [`_restart.md`](_restart.md) is the
modernisation reading order; it is still current and this file does not
supersede it. **Batches 1, 2 and 3 are done** — what they were and what they
taught is at the end of this file.

Three plan files changed with this proposal, and none of the changes re-scopes
an existing task. `ownership` gained **T10**, which owns the two calls its own
closing node could not have passed with; `strings-modernised` gained **T20**,
which owns the file that finding sits in; `language-hygiene/T10` gained the
file holding 62% of its work; and `determinism/T4` had a declared path
corrected that pointed at a file which does not exist. All four are described
below.

---

## Why this batch is shaped by plans rather than by tasks

The previous three batches picked the most valuable ready tasks. That was
right while there were forty of them. There are now **twenty open across five
plans**, and three of those plans are within a few nodes of done — so the
question worth asking changed from "what is the best task" to "**what closes a
plan**".

A closed plan is worth more than the sum of its tasks. It archives, it stops
being something every future batch has to re-read and re-measure, and — twice
in this tree — it turned out to be hiding work that only the closing node's
own acceptance criteria would have caught. That happened again while measuring
this batch, which is the finding below.

---

## Where every plan stands

Counted from the YAML at `97b5844`, after this proposal's three additions.
Six plans are complete and in `tasks/Archive/`; five are open.

| Plan | done | todo | Nodes between here and archived |
|---|---:|---:|---|
| the six archived plans | 87 | 0 | — |
| `determinism` | 2 | 2 | **2** — both ready, both parallel-safe, neither needs a build |
| `ownership` | 5 | 5 | **5** — a 3-deep chain, plus 2 that run beside it |
| `language-hygiene` | 9 | 3 | **3** — but two of them are joined at one file, see below |
| `strings-modernised` | 13 | 7 | 7 — two of them large; not closable this batch |
| `namespace-migration` | 2 | 3 | 3 — cannot start closing until the others do |

Twenty tasks open, thirteen offered by `--next`. All seven local checks pass at
`97b5844`, before and after this proposal's edits:

```
check_project_files  check_layering  check_task_dag  check_containers
check_math_types     check_format    check_hygiene            — all OK
```

> `check_format` compares against `origin/main`. A container that cloned before
> the last merge reports a failure that is entirely stale refs — `git fetch
> origin main` first, then re-run.

---

## The finding that shapes this batch

**`ownership/T7` — the node that closes migration stage 5 — could not have
passed, and would not have found that out until every other task in the plan
was done.**

Its acceptance is `grep -rwE "SAFE_DELETE|SAFE_FREE"` over the six projects
returning nothing. Run at `97b5844`, that grep returns four files:

| File | Sites | Macro | Owner |
|---|---:|---|---|
| `NeuronCore/NeuronCore.h` | 2 | both | the definitions — `T7` itself |
| `Species/App.cpp` | 17 | `SAFE_DELETE` | `ownership/T6` |
| `Species/GameCursor.cpp` | 10 | `SAFE_DELETE` | `ownership/T5` |
| **`NeuronClient/Shape.cpp`** | **2** | **`SAFE_FREE`** | **nobody — now `ownership/T10`** |

`ownership/T3` declared `Shape.cpp` and is done, legitimately: **its
acceptance grep reads `EmptyAndDelete|SAFE_DELETE` and does not name
`SAFE_FREE`.** The file passed a criterion that was never testing for the
thing left in it. Both greps are correct in isolation; the gap is that nobody
ran the *closing* one until the closing node was nearly reachable.

This is the same shape as Batch 3's finding — `\bsprintf\b` does not match
`vsprintf`, so `strings/T9` could have declared a tree-wide zero over eleven
unbounded `vsprintf` calls. Twice now, in two different plans, the terminal
"everything is gone" node has been checkable months before it was reachable
and nobody checked it. The rule worth taking from the second instance:

> **Run a closing node's acceptance grep on the day the node is WRITTEN.** It
> costs one command, it is the only criterion in a plan that is testable before
> its dependencies exist, and it is the one whose failure is most expensive —
> it surfaces when the plan is otherwise finished.

The two calls are in `ShapeMarker::~ShapeMarker`. They are also nearly free to
fix: the other two classes in the same file (`ShapeFragment` at :374/:376,
`Shape` at :1404) already free identically-shaped members with a bare `free()`,
so the macro is the odd one out in its own file. `ownership/T10` records that
as the likely answer and leaves the decision to whoever runs it.

**The second gap is behind the first.** Those members are `strdup`'d `char*`,
and `NeuronClient/Shape.cpp` holds **13 `strdup` calls — more than any other
file in the tree** — while `strings-modernised` declares `StaticShape.cpp` and
never this file. `strings/T9`'s grep names the strcpy and sprintf families, so
`strdup` is invisible to it too. That work is now **`strings/T20`**, which
carries the hazard that makes it non-mechanical: every comparison of these
names in the tree is `stricmp`, at six sites, and `std::string::operator==`
matches `strcmp`. A straight conversion silently narrows marker lookup, on
names that come from `GameData` files rather than from code.

---

## The proposal

### Batch 4 — close two plans, and clear the road to the third

| | Track | Tasks | What it finishes |
|---|---|---|---|
| **A** | `determinism` | `T3`, `T4` | **The whole plan.** Both are reading tasks that change no behaviour. |
| **B** | `ownership` | `T10`, `T5` → `T6` → `T7`, and `T8` | **The whole plan, and migration stage 5.** Also clears `namespace/T5`'s last cross-plan blockers. |
| **C** | `language-hygiene` | `T12` only, and only after Track B's `T5` | Leaves the plan at `T10` → `T11`, which is one batch of its own. |

**Track A is the one to start now, and it is the only plan-closure available
to an agent that cannot build Windows.** `determinism/T3` and `T4` verify with
`check_hygiene.py` and `check_format.py` — no msbuild, no vstest — because
their deliverable is an answer written into the plan and their acceptance
forbids a diff outside comments. Two commits close a plan and retire two
`AGENTS.md` *Known issues* bullets that have been open since 2026-08-02 and
2026-08-04 respectively.

They are disjoint from each other and from everything except `ownership/T8`,
which shares `SoundInstance.cpp` and `SoundSystem.cpp` with `T3`. That overlap
is deliberate and already recorded on both tasks: **same agent, separate
commits.** Whoever opens `SoundInstance.cpp` for the ownership conversion is
in the right place to answer the RNG question, and a lifetime conversion plus
an RNG question in one diff is the review nobody can judge.

**Track B is the spine, and it is strictly serial after its first node.**
`T10` (three lines, `parallel_safe: true`, no dependencies) can go first or
alongside. Then `T5` → `T6` → `T7`, each depending on the last. `T8` runs
beside the chain — it shares no file with `T5`, `T6` or `T7`.

Two things to know before starting the chain:

- **`T6` needs the owner.** Its acceptance requires the game to reach the main
  menu after *each* commit, and it is the highest-blast-radius ownership change
  in the tree — 17 `SAFE_DELETE`s across the whole subsystem graph, where
  construction and destruction order is load-bearing because subsystems reach
  each other during teardown. Plan for the gate; do not plan around it.
- **Two of `T5`'s eight declared files have nothing left matching its
  acceptance grep.** `LevelFile.cpp/.h` and `GlobalWorld.cpp` carry zero
  `SAFE_DELETE`/`EmptyAndDelete` at `97b5844` — but they carry 47 raw `new` and
  38 raw `delete` between them, which is what the task's *intent* is about. The
  grep is narrower than the task. Read the intent, not the acceptance line, or
  the two biggest files in the list look already done.

**Track C is one task, and its timing is the whole point.**
`language-hygiene/T12` is ready and `parallel_safe: true` — and it collides
with `ownership/T5` over **five** files: `GlobalWorld.cpp`, `LevelFile.cpp`,
`TaskManager.cpp`, `GameCursor.cpp` and `TaskManagerInterfaceIcons.cpp`. Both
are parallel-safe, which is once again exactly the wrong reason to run them
together. It goes after `T5` lands, or it goes in Batch 5.

### Not in the batch, and why

- **`language-hygiene/T10` and `T11`.** They are the rest of that plan and they
  should be a batch, not a track in this one. **They are joined at
  `ControlTypes.inc`:** of T10's 218 `INPUT_TYPE_` sites, **136 are in that
  one generated table**, and the table was declared by `T11` alone. T10 owns
  the second column, T11 owns the first, and the existing `T10` → `T11` edge is
  what keeps them from colliding. T10's file list is corrected here; the
  measurement is why it is not also in this batch.
- **`strings-modernised`.** Seven nodes, two of them large (`T12` at 225 call
  sites in 45 files, `T11` over the tree's most contended member name). It is
  not closable this batch under any ordering, so it contributes nothing to the
  plan-closing goal — but **`T18` and `T20` are both small, `parallel_safe:
  true`, and contested by nothing in Tracks A–C.** They are the two slots a
  spare agent can take without touching the batch.
- **`namespace/T2`.** Ready, sequenced last on purpose, and it declares whole
  directories so it contests everything by construction. Note what Track B buys
  it: `namespace/T5`'s only outstanding cross-plan blockers are `ownership/T5`
  and `T6`, so closing Track B leaves `namespace` a clean `T2` → `T4` → `T5`
  chain — the whole plan, in order, with nothing else in its way. **That makes
  `namespace-migration` the natural Batch 5 or 6, and the last plan standing.**
- **`strings/T17`.** Ready, and it contests `GlobalWorld.cpp` and
  `LevelFile.cpp` with `ownership/T5` and `language-hygiene/T12`. It is the
  three-way collision in this ready set; it wants a clear tree.

### One decision to put to the owner

**`ownership/T10` — bare `free()`, or convert the members?** Option 1 is three
lines, matches what the other two classes in the same file already do, and
unblocks `T7` immediately. Option 2 (`std::string`) is stage-4 work in a file
`strings-modernised` does not own, so taking it inside an `ownership` task
merges two migration stages in one diff, which this plan's own rule forbids.

Both are written into `T10`, and it is safe in either order — if `strings/T20`
lands first, `T10` closes as a no-op. **The recommendation is option 1**: it is
the smaller diff, it is what the file already does three lines away, and it
keeps the stage boundary the plans are built on. Recorded here as a decision
rather than taken silently, because "which plan owns this file" is exactly the
question the two findings above came from.

---

## The collision check

Thirteen ready tasks across five plans. `--next` says nothing about whether
they can run together, because it reasons per plan.

| Pair | Contested files |
|---|---|
| `language-hygiene/T12` × `ownership/T5` | **5** — GlobalWorld, LevelFile, TaskManager, GameCursor, TaskManagerInterfaceIcons |
| `language-hygiene/T12` × `strings/T17` | 2 |
| `ownership/T5` × `strings/T17` | 2 |
| `determinism/T3` × `ownership/T8` | 2 — deliberate; same agent, separate commits |
| `ownership/T10` × `strings/T20` | 1 — deliberate; safe in either order, never the same commit |
| `language-hygiene/T10` × `T12` | 1 — TargetCursor.cpp |
| `determinism/T4`, `strings/T18` × everything | **0** |

Reproduce with the script at the end of this file. Two cautions carry over:
`strings/T12` declares 2 files against a reach of 45, and `namespace/T2`
declares directories, so both under- and over-report by construction.

---

## What I actually ran

Linux, at `97b5844`. All seven Python checks, before and after the edits — all
pass, after `git fetch origin main`. `check_task_dag.py --next` on every plan,
before and after. The collision script over the thirteen ready tasks. Tree-wide
greps for `SAFE_DELETE`/`SAFE_FREE` (4 files, 31 sites), `EmptyAndDelete` (**7**
real call sites out of 16 apparent — the other nine are comments and the two
transitional definitions), `strdup` (23 files, `Shape.cpp` highest at 13),
`INPUT_TYPE_` split by extension (218 sites / 19 files, of which 136 in
`ControlTypes.inc`), and the seventeen `Camera::Mode` enumerators by name
(132 sites / 22 files, which matches `T12`'s declared list exactly).

**Two measurements in the previous proposal were reproduced and one was
corrected.** Camera::Mode's 22 files are right. The `Mode[A-Z]\w+` grep that
produced them over-reports by 16 files when run without the enumerator names —
`LaserFence`, `Tripod`, `LocationEditor` and eleven others have their own
`Mode*` enumerators. The declared list is right; the grep in the previous
file's table is not the one to re-run.

**Nothing here was built, and nothing was launched.** No MSVC and no Windows
client in this environment, so every size is a count of lines and call sites
rather than a compile, and the Garden smoke test remains the owner's. This
batch is a scheduling argument; it is not evidence that any of it compiles.

---

## What happened to Batches 1, 2 and 3

Kept because the sequence is the evidence.

**Batch 1** — `directxmath/T19`, `ownership/T4`, `strings/T8`, three agents in
parallel. T19 landed on CI 513 and T4 on CI 514. `strings/T8` was measured,
found to be a different task than its file list implied, and released unstarted
with the finding written into its plan entry.

**Batch 2** was proposed as `strings/T12` then `strings/T11`, and was executed
as `directxmath/T22` + `T23` instead: closing the owner gate made those ready,
and `strings/T12` — two declared files, 44 real ones — collided with them over
six `Species` files. **A batch proposal expires the moment a gate closes.**
Both math tasks landed, the plan finished at 28 of 28, and it is archived.

**Batch 3** was `strings/T8` → `T12` → `language-hygiene/T10`, and it executed
as `strings/T8` (CI 556) then `strings/T19` (CI 560) — because T8 split in two
on measurement, exactly as its own notes had proposed, though along a different
line than proposed. Fifteen files, 78 call sites, eleven new tests, x64 Debug
green and 180 tests passing on each. It also found the four unowned variadic
entry points that became `strings/T17` and `T18`, and it corrected `AGENTS.md`'s
test count, which had been wrong at 180 while CI counted 169.

Two things it left that this batch used: `FileWriter::printf` is pinned by
eight tests over the formats the writers actually use, and T19's accessor
finding — **every accessor that can fail is `char const*` for a reason; check
what its null means before converting its return type.**

---

## Reproducing the collision check

```python
# python3 - <tasks/_next-batch.md's script>  — run from the repo root
import yaml, glob, collections
ready = {  # refresh with: check_task_dag.py --next tasks/<plan>.yaml
    'determinism':           ['T3', 'T4'],
    'language-hygiene':      ['T10', 'T12'],
    'namespace-migration':   ['T2'],
    'ownership':             ['T5', 'T8', 'T10'],
    'strings-modernised':    ['T11', 'T12', 'T17', 'T18', 'T20'],
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

**That script reads declared `files` lists, and this batch found a third one
wrong** — after `language-hygiene/T10` (11 declared against 25) and
`strings/T12` (2 against 45), it is now `determinism/T4`, which declared
`Species/LandscapeRenderer.cpp`: **a path that does not exist.**
`layering-inversion` moved the renderer into `GameLogic` with the rest of the
world model — and it had already done so when T4 was written on 2026-08-05, so
this is not stale-plan rot. It is a wrong path passing validation in a task
written the same day this batch was scoped. Corrected here, along with its
`project`, which said `Species` for the same reason.

That one is worth a line of its own, because it is not the same mistake as the
other two. `check_task_dag.py` validates the GRAPH — ids, edges, acyclicity,
status transitions — and **not that a declared file is on disk**. A path that
rots when a file moves stays valid forever, and the first person to notice is
an agent opening a file that is not there. Every plan in `tasks/` predates at
least one of the moves that `layering-inversion` made; if you are about to
start a task written before 2026-08-02, `ls` its file list first.
