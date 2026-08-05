# Task DAG standard

How work on Species is broken down for agentic execution.

Anything larger than a single-file change is expressed as a **directed acyclic
graph** of tasks before any code is written. Tasks are the nodes, dependencies
are the edges, and the graph lives in a YAML file under `tasks/`.

The graph is not documentation of a plan — it *is* the plan. An agent reads it to
decide what to work on, updates it as work completes, and commits it alongside
the code.

---

## Why a DAG

A flat checklist encodes a total order that usually is not real. It forces
sequential execution where the work is independent, and it hides the cases where
step 7 genuinely cannot start until step 3 lands.

A DAG states dependencies explicitly, which buys three things:

- **Concurrency falls out of the structure.** Tasks with no path between them can
  run at the same time, by different agents, in different worktrees.
- **Readiness is computable.** "What should I do next" becomes a query, not a
  judgement call: `python3 tools/check_task_dag.py --next`.
- **The blast radius is bounded.** A task declares the files it expects to touch,
  so two concurrent tasks that would collide are visible before either starts.

Acyclicity is the invariant that makes the rest work. A cycle means the breakdown
is wrong — two tasks each need the other to finish first — and the validator
rejects it rather than letting an agent deadlock on it.

---

## When to write one

| Situation | Approach |
|---|---|
| One file, one concern, obvious verification | Just do it. No plan file. |
| Several files, or an ordering that matters | Write a plan under `tasks/`. |
| Anything that spans projects (layers) | Write a plan. Always. |
| A modernisation sweep over many files | Write a plan, one task per coherent group. |
| Work another agent may pick up | Write a plan. |

When in doubt, write the plan. It costs a few minutes and it is the artefact that
survives a context window ending mid-task.

---

## File layout

```
tasks/
  _template.yaml            copy this to start
  <plan-name>.yaml          one open plan per file, kebab-case, matching `plan:`
  Archive/
    <plan-name>.yaml        plans with nothing left open
```

A plan file is committed with the work it describes and stays in the tree after
completion, as the record of how something was done. Delete a plan only when its
subject matter no longer exists.

**When a plan has no task left in `todo`, `in_progress` or `blocked`, move it to
`tasks/Archive/`.** That keeps `tasks/` to the plans someone might still pick
up. Archived plans are still LOADED by `check_task_dag.py` and still resolve
`blocked_by` — **all 65 such edges point into them as of 2026-08-05**, because
every plan is now archived, and an edge into a plan the loader cannot see is an
unresolvable reference rather than a satisfied one. They are not validated or
reported by default, because a finished plan does not need re-listing on every
run.

> **As of 2026-08-05 `tasks/` holds no plan at all.** Eleven are in `Archive/`,
> every task in every one of them `done` or `abandoned`, and what is left beside
> `Archive/` is `_template.yaml` and the reading orders. The next piece of work
> starts by writing a plan, not by picking one up.

**Archiving is not one-way.** A finished plan whose subject turns out to have
unowned work in it comes back out of `Archive/` with that work as new tasks
rather than getting a near-duplicate plan file beside it. `determinism.yaml`
did exactly that on 2026-08-05: it closed at two tasks, was archived, and
reopened when the owner gave its two long-standing *Known issues* findings
scoping tasks. The rule is the same in both directions — a plan lives in
`tasks/` when something in it is open.

Moving a plan means updating the paths that name it. `grep -rn
'tasks/<plan>.yaml'` across `*.md`, `*.py` and `.github/` finds them; fourteen
files needed it for the first six, and four for `determinism` coming back.
**Run it after archiving, not before** — archiving `strings-modernised` left six
stale paths behind across `AGENTS.md`, `check_hygiene.py` and two of the reading
orders, none of which any check catches: a path in prose is a string, and
nothing validates it.

---

## Schema

```yaml
plan: restore-arm64-build            # required. kebab-case id, matches filename
title: Restore a clean ARM64 build   # required. one line, human-facing
phase: 2                             # optional. roadmap phase from AGENTS.md
summary: >                           # optional. why this plan exists
  Free text.

tasks:                               # required. non-empty list
  - id: T1                           # required. unique within the plan
    title: Short imperative phrase   # required
    intent: >                        # required. WHY, not how. One or two sentences.
      What changes about the system once this is done, and why that matters.
    project: NeuronCore              # optional. which layer this touches
    depends_on: []                   # optional. task ids in THIS plan. default []
    blocked_by:                      # optional. tasks in ANOTHER plan, plan/Tn
      - containers-replaced/T4       # see "Ordering across plans" below
    files:                           # optional but strongly encouraged
      - NeuronCore/Server.cpp        # the expected touch set; used to spot
      - NeuronCore/Server.h          # collisions between concurrent tasks
    acceptance:                      # required. non-empty. observable outcomes
      - Server.cpp no longer includes App.h or Globals.h
      - Neither NeuronCore.vcxproj nor Server.cpp names a project above NeuronCore
      - NeuronCoreTests covers the extracted seam
    verify:                          # optional. commands that prove acceptance
      - python3 tools/check_layering.py
      - msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64
      - vstest.console.exe ARM64\Debug\*Tests.dll /Platform:ARM64
    parallel_safe: true              # optional. default true. false = needs
                                     # exclusive access to the tree
    status: todo                     # required. see below
    notes: >                         # optional. findings, blockers, decisions
      Free text, appended to as work proceeds.
```

### Status values

| Status | Meaning |
|---|---|
| `todo` | Not started. |
| `in_progress` | An agent is actively working it. All dependencies are satisfied. |
| `blocked` | Cannot proceed for a reason outside the graph. `notes` must say why. |
| `done` | Every acceptance criterion is met and every `verify` command passes. |
| `abandoned` | Deliberately dropped. `notes` must say why. Counts as satisfied. |

The validator enforces that a task is only `in_progress` or `done` when all of its
dependencies are `done` or `abandoned`.

---

## Ordering across plans

`depends_on` names tasks **inside one plan**. That is deliberate — a wave is a
statement about a single plan, and a plan should be readable on its own.

But real orderings do cross plan files. The modernisation stages
(`CODING_STANDARDS.md`) run **per file**: a file finishes stage 3 before it
starts stage 4, before stage 5 — and those three stages live in
`containers-replaced.yaml`, `strings-modernised.yaml` and `ownership.yaml`.
Nothing in a single plan's graph can see that. Left in prose, it is an ordering
`--next` actively contradicts: it will offer a stage-5 task on a file whose
stage-3 task has not started.

`blocked_by` states it:

```yaml
  - id: T1
    title: Adopt unique_ptr in NeuronCore (Preferences, Profiler)
    blocked_by:
      - containers-replaced/T18      # stage 3 for these files
      - strings-modernised/T2        # stage 4 for these files
```

Each entry is `plan/Tn`, and the validator checks that the plan and the task both
exist, that the reference points **outside** its own plan (use `depends_on`
otherwise), and that no cycle forms once both edge kinds are taken together.

It gates readiness and status exactly like `depends_on`: a task with an unmet
blocker cannot be `in_progress` or `done`, and `--next` will not offer it. What
it does *not* do is change wave numbering, because waves are per plan. `--next`
lists such tasks separately instead of hiding them:

```
ownership: held by another plan
  T1  Adopt unique_ptr in NeuronCore (Preferences, Profiler)
      waits on containers-replaced/T18, strings-modernised/T2
```

**Use it for real ordering, not for narrative.** The test is the same as for
`depends_on`: if the second task could technically start before the first
finishes, do not add the edge. The two cases that earn one are per-file stage
order, and a task that moves files another plan names by path.

`blocked_by` is not a substitute for checking `files`. Two tasks in different
plans that list the same file still collide even with no ordering between them —
grep the other plans for your touch set before you start.

---

## Writing good tasks

**Acceptance criteria must be observable.** "Server.cpp is cleaner" is not a
criterion. "Server.cpp no longer includes App.h" is. If you cannot state how
someone else would check it, the task is not specified yet.

**Prefer `verify` commands over prose.** A criterion backed by a command an agent
can run is a criterion that cannot be fudged.

**A task that adds or changes behaviour states its tests in `acceptance`, and
runs them in `verify`.** Not as a separate downstream node — a node that lands
code and defers its tests is a node that will be marked `done` while the tests
never arrive. The exception is characterising legacy behaviour before a
conversion, which genuinely is its own task and its own commit: there the test
node comes *first* and the conversion depends on it. What earns a test is in
[`TESTING.md`](TESTING.md); the short version is that anything on the wire and
anything the simulation depends on being identical everywhere earns one.

If a task's honest answer is that nothing it touches can be tested yet — the
`GameLogic` link wall, a layer that is still a stub — say that in `notes`. "Not
tested because X" is a finding. Silence is indistinguishable from having not
thought about it.

**One reviewable change per task.** If a task would produce a diff too large to
review in one sitting, split it. The DAG is the right place to express that split.

**Edges mean "cannot start until", not "reads better after".** A dependency you
added for narrative tidiness is a dependency that needlessly serialises the work.
If T2 could technically start before T1 finishes, do not add the edge.

**Declare `files`.** Two tasks in the same wave that both list
`NeuronCore/Server.cpp` will conflict. Listing the touch set makes that visible
before two agents start editing the same file.

**Run a closing node's acceptance grep the day you WRITE it.** A plan that ends
with "grep X over the tree returns nothing" has one criterion that is testable
immediately, years before its dependencies exist — and it is the criterion whose
failure costs the most, because it surfaces when the plan is otherwise finished.
Two plans here were unfinishable for months and neither knew it:
`strings-modernised/T9` greps `\bsprintf\b`, which does not match `vsprintf`,
so eleven unbounded calls were invisible to the node that declares stage 4 over;
`ownership/T7` greps `SAFE_DELETE|SAFE_FREE`, and the only `SAFE_FREE` calls
left in the tree sat in a file whose own task's grep named `SAFE_DELETE` alone.
Both were found by running the closing grep, once, while scoping a batch. Run
it, list what it returns, and give every hit an owning task before the plan is
committed.

**A declared path is not checked against the disk.** `check_task_dag.py`
validates the graph — ids, edges, acyclicity, status transitions — and nothing
about whether `files` entries exist. A path that rots when a file moves stays
valid forever, and the first person to find out is an agent opening a file that
is not there. `determinism/T4` declared `Species/LandscapeRenderer.cpp`, a path
`layering-inversion` had already retired before the task was written — so this
is not only rot in old plans, it is a wrong path passing validation in a plan
written the same week. `ls` a task's file list before starting it.

**Intent is the part that survives.** By the time someone reads the plan again,
the code has moved on. `intent` is what tells them whether the task still makes
sense.

---

## Working a plan

```bash
# What can I start right now?
python3 tools/check_task_dag.py --next tasks/restore-arm64-build.yaml

# What could run concurrently, and in what order overall?
python3 tools/check_task_dag.py --waves tasks/restore-arm64-build.yaml

# Validate every plan in the tree (CI runs this)
python3 tools/check_task_dag.py

# Graph for a PR description or a doc
python3 tools/check_task_dag.py --mermaid tasks/restore-arm64-build.yaml
```

The execution loop for an agent:

1. `--next` to find a ready task. If several are ready and they do not share
   `files`, they may be worked concurrently.
2. Set that task's `status` to `in_progress` and commit that change *first*, so a
   parallel agent does not pick up the same task.
3. Do the work.
4. Run every command under `verify`. All must pass.
5. Check each `acceptance` criterion by hand.
6. Set `status: done`, record anything surprising in `notes`, and commit the plan
   update together with the code.

If a task turns out to be wrong — the intent no longer holds, or the breakdown was
mistaken — do not quietly reshape it. Set it `blocked` or `abandoned` with a note,
and add the corrected tasks. The graph is a record as well as a plan.

### An edge orders the work; it does not promise a buildable state between

`depends_on` says B is done after A. It does NOT say the tree compiles in
between, and the difference has bitten once.

`namespace-migration` T4 put `GameLogic` in `namespace Species` and T5 put the
`Species` executable in the same namespace. Landing T4 alone leaves every
unqualified GameLogic name in the executable unresolvable, because there is no
using-directive for the game namespace — a tree that does not build. The two
landed in one commit, with the reason recorded on both tasks.

When you find a pair like that, say so in the notes rather than splitting the
commit to match the graph. And record the ordering that WOULD have had
buildable intermediates if there is one: for those two it was the reverse,
because the executable inside the namespace still finds global GameLogic names
by ordinary outward lookup.

**A file list is not a promise either.** Eight declared lists in this tree have
been found wrong — one naming a file that does not exist, one naming a file with
none of the work in it, and six under-reporting by between three and six times.
`ls` the list and measure the reach before starting; `tasks/_next-batch.md`
carries the method and the tally.

---

## Waves and concurrency

`--waves` computes the topological levels of the graph. Every task in a wave has
all of its dependencies in earlier waves, so a wave is a set that may be executed
in parallel.

```
wave 1 (3 tasks, may run concurrently):
  [todo       ] T1  Move Team ownership out of NeuronCore
  [todo       ] T4  Add NetworkUpdate round-trip coverage
  [todo       ] T7  Split Preferences off the client layer
```

Note that two tasks in the same wave may both add files to the same
`Tests/<Name>Tests` project without conflicting, as long as they add *different*
files — which is the usual case, since test files are named for what they cover.
Both will touch that project's `.vcxproj` and `.vcxproj.filters` though, so list
those in `files` and expect to resolve a small merge there.

Waves are computed from `depends_on` alone. A task held by a `blocked_by` keeps
its wave number and is annotated `waits on …`, because its position in *this*
plan has not changed — only its start time has.

Two caveats the tool cannot check for you:

- Tasks in the same wave that list overlapping `files` will conflict. Serialise
  them with an edge, or work them in separate worktrees and merge deliberately.
  The same is true of tasks in *different* plans that list the same file; see
  [Ordering across plans](#ordering-across-plans).
- A task marked `parallel_safe: false` needs the tree to itself — a
  repo-wide reformat, a toolset bump. Run it alone.

---

## Example

`tasks/_template.yaml` is a minimal, valid starting point. For a worked example
grounded in real numbers from this repository, see
`tasks/Archive/neuroncore-layering.yaml` — the plan for removing the thirty upward
includes that make `NeuronCore` depend on the client, game and app layers.
