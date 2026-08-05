# The next implementation batch

Written 2026-08-05 at `a2d78c6`, the merge that closed the T19 partial run. This
is a proposal, not a plan — the plans are the ten YAML files beside it and none
of them changes here. It answers one question: **of everything that is ready,
what should the next batch be, and why that rather than the rest.**

Read [`AGENTS.md`](../AGENTS.md) first. [`_restart.md`](_restart.md) is the
modernisation reading order and [`_restart-directxmath.md`](_restart-directxmath.md)
is the math migration's; both are still current and this file supersedes
neither. It sits on top of them because they were written per-plan, and the
question of what to do next is now a cross-plan one.

---

## Where every plan stands

Counted from the YAML at `a2d78c6`, not inherited from prose.

| Plan | done | todo | other | State |
|---|---:|---:|---|---|
| `neuroncore-layering` | 13 | 0 | — | complete |
| `layering-inversion` | 13 | 0 | 5 abandoned | complete |
| `containers-replaced` | 24 | 0 | — | complete |
| `rename-scaffolding` | 5 | 0 | — | complete |
| `rename-darwinian` | 4 | 0 | — | complete |
| `directxmath-migration` | 18 | 10 | — | **T19 is the only ready code task** |
| `strings-modernised` | 11 | 5 | — | T8, T11, T12 ready |
| `ownership` | 4 | 5 | — | T4, T8 ready |
| `language-hygiene` | 9 | 3 | — | T10, T12 ready |
| `namespace-migration` | 2 | 3 | — | T2 ready, sequenced last by design |
| `determinism` | 1 | 0 | 1 blocked | T2 is an owner gate |

Twenty-three tasks open. Eleven of them are offered by `--next` today.

---

## What is ready, measured

Every number here was produced on Linux at `a2d78c6` by the command in the last
column. None of it was built.

| Task | Size | Command |
|---|---|---|
| `directxmath/T19` | **~407 edit points** left — Weapons.h/.cpp ~246, Rocket.h/.cpp ~161. **87 of the tree's 198 `AsLegacy` uses** are in these two files. | see T19 `notes`; `grep -rEo AsLegacy` |
| `ownership/T4` | 6 real `EmptyAndDelete` sites over **5 members, ~80 uses**. 7 files. | `grep -cw EmptyAndDelete` |
| `strings/T8` | **43** `strcpy`-family calls over 3 files (GlobalWorld 18, LevelFile 17, Script 8). | `grep -cE 'strcpy\|…'` |
| `strings/T11` | **6** fixed `char[N]` members over 3 headers; 8 files. | `grep -nE 'char\s+m_\w+\s*\['` |
| `strings/T12` | **10** variadic entry points, **225** call sites in **44** files. | `grep -rEo 'DrawText…\s*\('` |
| `language-hygiene/T10` | **218** `INPUT_TYPE_` sites, 11 files. | `grep -rEo '\bINPUT_TYPE_\w+'` |
| `language-hygiene/T12` | **232** `Mode*` sites, 21 files. | `grep -rEow 'Mode[A-Z]\w+'` |
| `ownership/T8` | 2 `EmptyAndDelete` in a 2206-line file, and **an ownership decision before any of it**. | T8 `notes` |
| `namespace/T2` | whole of `NeuronClient`. Deliberately last. | — |

All seven local checks pass at `a2d78c6`:

```
check_project_files  check_layering  check_task_dag  check_containers
check_math_types     check_format    check_hygiene            — all OK
```

---

## The proposal

### Batch 1 — three tasks, in parallel

`directxmath/T19`, `ownership/T4`, `strings/T8`. All three are
`parallel_safe: true`, and **their file lists are disjoint** — checked, not
assumed; see *The collision* below.

| | Task | Why it is in the batch |
|---|---|---|
| 1 | `directxmath/T19` — Weapons and Rocket | **The highest-leverage node in the tree.** It is the last code task in wave 5, and wave 6 is `T21`, the owner's Garden smoke test. **Nine of the ten remaining math tasks sit behind T21** — T22, T23, then T12 and T20, then T28, then T25, then T26 and T27. Nothing else in the tree unblocks that much. |
| 2 | `ownership/T4` — unique_ptr in GameLogic | Stage 5's only unblocked node. It unblocks `ownership/T6` (with T5) and is one of two `blocked_by` edges on `namespace/T4`. |
| 3 | `strings/T8` — the level and profile writers | The other half of that funnel: `ownership/T5` is `blocked_by: strings-modernised/T8` and cannot start until it lands, and T5 is T6's other dependency. It also feeds `strings/T9`. |

Together T4 and T8 are the whole of what stands between today and stage 5's tail
(`ownership` T5 → T6 → T7) and the namespace work behind it. They are small —
43 string calls and ~80 pointer uses — and they are the two nodes everything
downstream funnels through, which is the same shape `strings/T5` had before it
landed.

**Sequencing within the batch.** T19's two file pairs are independent of each
other and belong in separate commits — Weapons.h's `Rocket` class is implemented
in Weapons.cpp and has nothing to do with Rocket.h's fuel buildings. Do Rocket
first: it is a third smaller and carries 16 `AsLegacy` uses against Weapons.cpp's
71.

**One cost that is already paid.** All four T19 files are clang-format clean at
`a2d78c6` — `clang-format <file> | diff - <file>` is empty for each. The
whole-file reformat commit the earlier T19 files needed is **not** needed here.

**One hazard that is not.** `ownership/T4`'s `m_positionHistory` means four
different things in four classes and `check_containers.py` skips contended names
rather than guessing, so it will not catch a mistake there. Virii is simulation
code walked from `Advance` and erased mid-tick; T4's own notes say do it last and
state the free point per site. That still applies.

### The gate — one owner run, not two

`determinism/T2` and `directxmath/T21` are **the same seven-step Garden run on
the same build**, and they should be asked for once.

`determinism/T1` changed `Spirit::Advance`'s draw count from the synchronised
stream, and T2 is the smoke test that confirms it. `Spirit.cpp` has since also
been converted to native math inside T19's partial run. So the file already
carries both changes, unverified, exactly as T19's own notes warned it would:

> Spirit.cpp was changed by determinism.yaml T1 and its smoke test (T2 there) is
> still open. If that gate has not been run when this task starts, this
> conversion lands a second unverified simulation change on the same file.

That has happened. It is not a defect — nothing was done out of order, the two
plans simply do not share a graph — but it means one run now closes both, and
running them separately would cost the owner a second pass over the same seven
steps for no extra information.

**Ask for the run once, after Batch 1 lands, and record it against both tasks.**
T21's acceptance is the stricter of the two (step 5 for the full thirty seconds,
step 7 explicitly checked), so satisfying T21 satisfies T2.

### Batch 2 — two tasks, each alone

Both are `parallel_safe: false` and want the tree to themselves. Run them after
Batch 1, in this order, while the owner gate is outstanding — neither touches
anything Batch 1 does.

| | Task | Why in this order |
|---|---|---|
| 4 | `strings/T12` — the TextRenderer variadic API | **The most valuable single task in the modernisation plans, and it is ready now.** 225 `printf`-style call sites in 44 files become `std::format`, which rejects at compile time what `printf` accepts and corrupts at run time. Every call site that fails to compile is a latent bug surfaced, not a conversion error — record them as found. It is also half of `strings/T13`'s dependency. |
| 5 | `strings/T11` — the Eclipse widget names | The other half of T13. Six `char[N]` members; the blast radius is name lookup. |

**T11's recorded hazard is smaller than advertised, and that is worth knowing
before scoping it.** `_restart.md` warns that Eclipse looks widgets up with
`strcmp` in some places and `stricmp` in others, and that `std::string::operator==`
matches the first and not the second. In T11's eight files there is **no
`stricmp` at all** — every lookup in `Eclipse.cpp`, `EclWindow.cpp`,
`EclButton.cpp`, `WindowManager.cpp` and `ScrollBar.cpp` is `strcmp`. The tree
has 225 `stricmp` calls, but none of them is on a widget name. So T11's fourth
acceptance line — *Eclipse matches names case-sensitively today and must
continue to* — is satisfied by a straight `operator==`, and the call sites still
need reading, but for the reason every conversion does rather than for this one.

### Not in the batch, and why

- **`language-hygiene/T12` (Camera::Mode).** Ready and `parallel_safe: true`,
  but see *The collision*. Run it after Batch 1, alone or with T10.
- **`language-hygiene/T10` (InputType).** Ready, `parallel_safe: false`, and
  file-disjoint from everything else ready. It is a fine fifth agent if the
  `parallel_safe: false` flag is read as "no repo-wide edit" rather than "no
  concurrent agent" — but the flag means the tree to itself, so take it at its
  word and run it in Batch 2 rather than reinterpreting it.
- **`ownership/T8` (SoundInstance).** Not schedulable as work yet. Its blocker
  is a **decision**: `ShutdownSound` serves both registered and unregistered
  instances and deletes unconditionally, and it tests `ValidIndex(m_id.m_index)`
  without checking the slot still holds *this* instance. Adding that check is a
  behaviour change and probably a fix. Get the decision first; the conversion
  after it is small. This is the same file that holds the unfixed
  `speciesRandom()` draws in *Known issues*, so whoever takes the decision is
  already in the right place to scope that too.
- **`namespace/T2`.** Ready, but the whole plan is sequenced last on purpose:
  pure churn over files that should have stopped moving. Every file Batch 1 and
  Batch 2 touch is a file it would have to move.

---

## The collision

One finding that changes the batch, and it is the reason this file exists rather
than a paragraph in `_restart.md`.

`--next` offers eleven tasks across five plans and says nothing about whether
they can run together, because it reasons per plan. Cross-referencing the `files`
lists of all eleven finds exactly **five contested files, and every one of them
is `language-hygiene/T12`**:

| File | Claimed by |
|---|---|
| `GameLogic/GlobalWorld.cpp` | `language-hygiene/T12`, `strings/T8` |
| `GameLogic/LevelFile.cpp` | `language-hygiene/T12`, `strings/T8` |
| `Species/Script.cpp` | `language-hygiene/T12`, `strings/T8` |
| `GameLogic/InsertionSquad.cpp` | `language-hygiene/T12`, `ownership/T4` |
| `GameLogic/TaskManager.cpp` | `language-hygiene/T12`, `directxmath/T19` |

`Camera::Mode` is reached from 21 files spanning three projects, so T12 collides
with all three Batch 1 tasks at once and with nothing else. Every other pair
among the eleven ready tasks is disjoint. That is what makes the three-agent
batch above safe and T12 the one to hold back — not its size, which is
comparable, and not its dependencies, which are empty.

Reproduce it with the script at the end of this file if the batch changes.

---

## Two housekeeping items, cheap and worth doing with the batch claim

Neither is a task in any plan. Both are places where the tree has moved past
what a document says.

**1. `AGENTS.md` points at a completed task for work that is still owed.** The
test-suite bullet says entity and building behaviour is "finally testable —
nobody has written those tests yet, which is `tasks/layering-inversion.yaml`
T11." T11 is **done**: it landed with T15 and wrote ten tests. The observation
behind the sentence is still true — `EntityGridTests` covers spatial indexing
and `RoutingSystemTests` covers waypoint ordering, and neither is entity or
building behaviour — but the pointer is wrong, and a reader who follows it finds
a closed task and concludes the work is done. Either restate it as an unowned
gap or give it a task. It should not keep naming T11.

**2. The two determinism findings in `AGENTS.md` have no task.**
`SoundInstance.cpp`'s two `speciesRandom()` draws on client-configuration-
dependent paths, and `LandscapeRenderer::GetLandscapeColour` reseeding the
simulation RNG from rendering code. Both are written up as *Known issues*, both
are explicitly not modernisation work, and `_restart-directxmath.md` says the
second "belongs in `tasks/determinism.yaml` once somebody establishes what it
costs". `determinism.yaml` has two tasks and one of them is an owner gate, so
the file is effectively idle and is the right home. **Scoping them is a reading
task, doable on Linux, and it is the prerequisite either way** — neither can be
fixed before somebody establishes whether it is benign. Add them as scoping
tasks rather than fixing tasks.

**3. The seam worklist in `_restart-directxmath.md` is one task stale.** It says
`grep -rl AsLegacy` is the live worklist and gives 23 files with 56 uses in
`Weapons.cpp`. At `a2d78c6` it is **20 files** (excluding `NeuronCore/Vector3.h`,
which defines the seam rather than using it) and **198 uses**, 71 of them in
`Weapons.cpp` — the earlier figure counted matching *lines*, and several lines
carry two. The count moved because T19's partial run took Spirit's nine. One
line to correct, and the distinction between lines and uses is worth keeping
because T25 cannot land until the number is zero.

All three are documentation-only and belong in the same commit that claims
Batch 1, not in a batch of their own.

---

## What I actually ran

Linux, at `a2d78c6`. All seven Python checks (`check_project_files`,
`check_layering`, `check_task_dag`, `check_containers`, `check_math_types`,
`check_format`, `check_hygiene`) — all pass. `check_task_dag --next` and
`--waves` on all ten plans. `clang-format 18.1.3` against the four T19 files.
The greps quoted in the tables above.

**Nothing here was built, and nothing was launched.** No MSVC and no Windows
client in this environment, so every size in this file is a count of lines and
call sites rather than a compile, and the Garden smoke test remains the owner's.
The batch above is a scheduling argument; it is not evidence that any of it
compiles.

---

## Reproducing the collision check

```python
# python3 - <tasks/_next-batch.md's script>  — run from the repo root
import yaml, glob, collections
ready = {  # refresh with: check_task_dag.py --next tasks/<plan>.yaml
    'directxmath-migration': ['T19'],
    'language-hygiene':      ['T10', 'T12'],
    'namespace-migration':   ['T2'],
    'ownership':             ['T4', 'T8'],
    'strings-modernised':    ['T8', 'T11', 'T12'],
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
