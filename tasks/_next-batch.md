# The next implementation batch

Written 2026-08-05 at `4a40231`, after Batch 4 executed. This is a proposal,
not a plan — the plans are the five YAML files beside it. It answers one
question: **of everything that is ready, what should the next batch be, and why
that rather than the rest.**

Read [`AGENTS.md`](../AGENTS.md) first. [`_restart.md`](_restart.md) is the
modernisation reading order; it is still current and this file does not
supersede it. **Batches 1 to 4 are done** — what they were and what they taught
is at the end of this file.

---

## Where every plan stands

Counted from the YAML at `4a40231`. Six plans are complete and in
`tasks/Archive/`; five are open with **sixteen tasks** between them.

| Plan | done | todo | What is left |
|---|---:|---:|---|
| the six archived plans | 87 | 0 | — |
| `determinism` | 5 | 1 | **T6 only, and it is yours** — the smoke test gating T5's RNG change |
| `ownership` | 8 | 3 | T11, then T6 (yours) → T7. Stage 5 ends here. |
| `language-hygiene` | 10 | 3 | T10 → T11, and T13 |
| `namespace-migration` | 2 | 3 | T2 → T4 → T5, and T5 waits on `ownership/T6` |
| `strings-modernised` | 14 | 6 | the largest remaining plan |

All seven local checks pass at `4a40231`, and CI is green on it.

> `check_format` compares against `origin/main`. A container that cloned before
> the last merge reports a failure that is entirely stale refs — `git fetch
> origin main` first, then re-run.

---

## The finding that shapes this batch

**The tree is now gated on the owner in two places, and almost nothing else is
gated on anything.**

Batch 4 finished the conversions that were blocking each other. What it left is
a ready set of ten tasks that contest, in total, **two files**:

| Pair | Contested files |
|---|---|
| `ownership/T11` × `strings/T17` | 1 — `GlobalWorld.cpp` |
| `language-hygiene/T13` × `strings/T17` | 1 — `LevelFile.cpp` |
| every other pair | **0** |

Compare Batch 4, where five ready tasks contested nine files and the batch
could not fan out at all. **This one can**, and it is the first that could
since Batch 1.

What it cannot do is finish anything without you. Two of the sixteen open tasks
are owner work rather than agent work, and both sit on critical paths:

- **`determinism/T6`** — the Garden smoke test against `determinism/T5`, which
  moved six simulation-state draws onto the synchronised RNG. That change is
  already in `main`. Until the run happens the plan cannot close, and nobody
  knows whether the simulation still behaves.
- **`ownership/T6`** — App owning its subsystems. Its acceptance requires the
  game to reach the main menu **after each commit**, and it is the highest
  blast-radius change in the tree: 17 `SAFE_DELETE`s across the whole subsystem
  graph, where construction and destruction order is load-bearing because
  subsystems reach each other during teardown. `ownership/T7` is behind it, and
  so is `namespace/T5`.

**Migration stage 5 cannot end without that second run.** That is the single
most useful thing to know when planning around this batch.

### What stage 5 has left, measured

```
EmptyAndDelete   ZERO call sites. Only the two transitional definitions
                 remain, in SlotMap.h and VectorUtils.h — both dead code now,
                 and both can go in T7.
SAFE_FREE        ZERO call sites.
SAFE_DELETE      17, all in Species/App.cpp, plus the NeuronCore.h definition.
```

---

## The proposal

### Batch 5 — three agent tasks that genuinely fan out, and two gates to open

| | Task | Why it is in the batch |
|---|---|---|
| 1 | `ownership/T11` — unique_ptr in GlobalWorld | The half of T5 split out for reviewability. 23 raw `new`, 19 raw `delete`, five owning vectors, three subsystem members — all shapes T5 already converted, so it is the best-understood task in the ready set. |
| 2 | `language-hygiene/T10` — InputType, and delete the dead `ControlTypes.cpp` | **File-disjoint from every other ready task**, measured. It unblocks `lh/T11` (473 sites), the last node in that plan. |
| 3 | `language-hygiene/T13` — the transition enum and its wrong bound | Small and self-contained, four files. Its only contested file is `LevelFile.cpp`, with `strings/T17`, which is not in this batch. |

**These three can run at once.** `ownership/T11` touches `GlobalWorld.cpp/.h`;
`lh/T10` touches the input drivers and `ControlTypes.inc`; `lh/T13` touches
`LevelFile`, `CameraAnimWindow` and `Camera.cpp`. No overlap between any pair.

**Their `parallel_safe` flags say otherwise, and the flags are wrong rather
than the measurement.** All three are `parallel_safe: false`, set when the tree
was more contested than it is now. Do not silently flip them — measure, then
update the flag in the same commit as the work, with the measurement in notes.
That is how `lh/T10`'s note already reads.

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

### Not in the batch, and why

- **`strings/T17` (FileWriter's variadic printf).** Ready, and the only task
  contesting anything — both contested files in the whole ready set are its. It
  wants a clear tree, and it is the obvious first task of Batch 6.
- **`strings/T11` and `T12`.** Ready and large: T12 is 225 call sites in 45
  files and splits into two populations needing different treatments; T11 is
  over the tree's most contended member name. Each is a batch's worth alone.
- **`strings/T18`.** Small, `parallel_safe: true`, contested by nothing — the
  one slot a fourth agent could take without any measurement.
- **`ownership/T6`, `determinism/T6`.** Owner work, above.
- **`namespace/T2`.** Ready, sequenced last on purpose, and it declares whole
  directories so it contests everything by construction. Note what this batch
  buys it: after `ownership/T6`, `namespace` is a clean `T2` → `T4` → `T5`
  chain with nothing else in its way — the whole plan, in order. **It is the
  last plan standing** and wants planning as a project rather than as a batch
  entry.

---

## Progress — what Batch 4 did

Six tasks landed: `determinism/T3` and `T4`, `ownership/T5`, `T8` and `T10`,
`strings/T20` and `language-hygiene/T12`. Neither plan it was organised around
closed, and both reasons were findings rather than slippage.

**THE DETERMINISM WORK WAS DONE TWICE, CONCURRENTLY.** This branch and `main`
both took `determinism/T3` and `T4` on the same day, without either knowing
about the other, and reached the same answer independently: `speciesRandom()`
is not the lockstep RNG, `syncrand()` is, and `MathUtils.h` has said so since
the code was inherited. `main`'s version went further — **six**
simulation-state draws on the unsynchronised generator where this branch found
four — and is the one that survives. The two it found and this branch missed
came through `frand()`/`sfrand()` rather than `speciesRandom()` directly,
including `LaserFence.cpp:66`, an outright desync. This branch's own proposal
had written *"ONE TRAP: `frand()` and `sfrand()` are speciesRandom in disguise
… a grep for the two names is not optional"* and then did not run that grep.

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
  the stack in `GlobalWorld.cpp`.
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

## The collision check

Ten ready tasks across five plans, contesting two files. Reproduce with the
script at the end of this file.

Two cautions carry over: `strings/T12` declares 2 files against a reach of 45,
and `namespace/T2` declares directories, so both under- and over-report by
construction. And a third, new one: **`parallel_safe` flags in this tree lag
reality.** Three of the ready set are flagged `false` and are provably disjoint
today. Trust a fresh measurement over the flag, and correct the flag when you
find it wrong.

---

## What I actually ran

Linux, at `4a40231`. All seven Python checks. `check_task_dag.py --next` on
every plan. The collision script over the ten ready tasks. Tree-wide greps for
`SAFE_DELETE`/`SAFE_FREE` (2 files: `App.cpp`, and the definition),
`EmptyAndDelete` (zero call sites), `hygiene-ok` (zero) and `TEST_METHOD`
(185).

**Nothing here was built, and nothing was launched.** No MSVC and no Windows
client in this environment. Batch 4's code changes were verified by CI rather
than locally, and the Garden smoke test remains yours.

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
zero tree-wide, retire the last `hygiene-ok` marker and add five tests. **The
recurring lesson across all four is the same one:** a batch proposal is a
measurement with a short half-life, and every batch so far has found its own
premise partly wrong on contact.

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

**That script reads declared `files` lists, and four have now been found
wrong** — `language-hygiene/T10` (11 declared against 25, plus the `.inc`
holding 62% of its work), `strings/T12` (2 against 45), `determinism/T4` (a
path that did not exist), and `language-hygiene/T12` (a false positive from a
shared enumerator name). `check_task_dag.py` validates the graph, not whether a
declared file is on disk. `ls` a task's file list before starting it, and
measure its reach with a grep for what the tree actually spells.
