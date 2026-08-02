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
  <plan-name>.yaml          one file per plan, kebab-case, matching `plan:`
```

A plan file is committed with the work it describes and stays in the tree after
completion, as the record of how something was done. Delete a plan only when its
subject matter no longer exists.

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
    depends_on: []                   # optional. list of task ids. default []
    files:                           # optional but strongly encouraged
      - NeuronCore/Server.cpp        # the expected touch set; used to spot
      - NeuronCore/Server.h          # collisions between concurrent tasks
    acceptance:                      # required. non-empty. observable outcomes
      - Server.cpp no longer includes App.h or Globals.h
      - tools/check_layering.py reports 2 fewer allowlisted violations
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

Two caveats the tool cannot check for you:

- Tasks in the same wave that list overlapping `files` will conflict. Serialise
  them with an edge, or work them in separate worktrees and merge deliberately.
- A task marked `parallel_safe: false` needs the tree to itself — a
  repo-wide reformat, a toolset bump. Run it alone.

---

## Example

`tasks/_template.yaml` is a minimal, valid starting point. For a worked example
grounded in real numbers from this repository, see
`tasks/neuroncore-layering.yaml` — the plan for removing the thirty upward
includes that make `NeuronCore` depend on the client, game and app layers.
