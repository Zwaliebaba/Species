# Restarting the migration

Written 2026-08-03 at `b0bde71`, the commit the work stopped on. This is not a
new plan — the plans exist and most of them are finished. It is the ordering
for picking the remaining ones back up, and the repair of the one place the
graph was known to be wrong.

Read `AGENTS.md` first. It is still the orientation document; this file only
answers "where were we, and what do I do next".

---

## Where it stopped

Six plans are complete: `neuroncore-layering`, `rename-scaffolding`,
`layering-inversion`, `containers-replaced`, and — as of the owner's smoke-test
run at `b0bde71` — `rename-darwinian`. Stage 3 is done: the legacy container
headers are deleted, not merely unused.

Four plans are open, with **19 tasks left** between them:

| Plan | Open | State |
|---|---|---|
| `strings-modernised` | 9 of 16 | Stage 4. Was mid-flight when work stopped. |
| `ownership` | 5 of 7 | Stage 5. Two landed, the rest gated on stage 4. |
| `language-hygiene` | 4 of 9 | Two sweeps landed; the enums and the min/max macros are left. |
| `namespace-migration` | 3 of 5 | Sequenced last by design. |

It did not stop because something broke. The last four commits are notes,
documentation and a scope correction — `80f4c92` *Record what T5 actually is,
and leave it stashed rather than half-pushed*. Stage 4 ran into a task whose
shape was wrong, the attempt was discarded deliberately, and the finding was
written down instead. That finding is the reason this file exists.

### Where the tree actually is

Re-measured at `b0bde71` on Linux. Commands are the ones to re-run rather than
trust; every one of these numbers will have moved by the time it matters.

| Axis | At `11aee84` | Now | Command |
|---|---|---|---|
| `strcpy` family | 367 sites | **180**, 53 files | `grep -rEow 'strcpy\|strncpy\|strcat\|sprintf\|snprintf' <projects>` |
| … by project | | GameLogic 152, Species 14, NeuronClient 13, NeuronServer 1, NeuronCore 0 | |
| `NULL` | ~578 | **4** — two documented exceptions, a comment, a string literal | `grep -rw NULL <projects>` |
| `#ifndef _included` guards | 223 | **0** | `grep -rl '#ifndef _included' <projects>` |
| Plain `enum` in headers | 12 | **11** to convert (one is already `enum class`) | `grep -rE '^\s*enum\s+[A-Za-z]' --include='*.h'` |
| Bare `min(`/`max(` | 216 | **216**, 52 files | `grep -rEo '(^\|[^:_A-Za-z0-9])(min\|max)\s*\(' <projects>` |
| `EmptyAndDelete` | 26 files | **13** call-site files | `grep -rlw EmptyAndDelete <projects> --include='*.cpp'` |
| `SAFE_DELETE`/`SAFE_FREE` | 35 | **38** across 6 files incl. the definition | `grep -rEow 'SAFE_DELETE\|SAFE_FREE' <projects>` |
| Raw `new` / `delete` in `.cpp` | 815 / 246 | **805 / 287** (token counts, includes comments) | |
| Files in `namespace Neuron` | — | **13** | `grep -rl 'namespace Neuron' <projects>` |

Stage 4 is half done and stage 5 has barely started. The `new`/`delete` counts
have not moved, which is expected: `ownership` T1 and T2 covered NeuronCore and
NeuronServer, about 1% of the tree between them.

---

## The one thing that was wrong, and what was done about it

`strings-modernised` T5 — *convert the GameLogic windows* — was attempted and
discarded. Its note is the deliverable, and its conclusion was:

> So T5, T6's three members, T8 and T11 are ONE coupled change wearing four
> task numbers. The DAG says they are independent and it is wrong. Whoever
> picks this up should either merge them or give them an explicit order.

That is verified, not inherited. `InputField` writes into a caller-registered
buffer through a raw `char*`, so `m_string` becoming a `std::string*` forces
seven members to convert in the same commit —
`CameraMount::m_name`, `CameraAnimation::m_name`,
`ScriptTrigger::m_scriptFilename`, `GenericHub::m_shapeName`,
`StaticShape::m_shapeName`, `FenceSwitch::m_script`, and the two editor
statics. `RegisterString` has three call sites; `CreateValueControl` takes
`void*` and has 84. Two of those seven members are what `strings` T6
deliberately left as fixed arrays; five of them are written to level files,
which is T8's byte-identity requirement.

**The choice made here was order, not merge**, because merging produces one
diff spanning GameLogic and Species with a byte-identity proof inside it, and
"one reviewable change per task" is the rule that breaks first. The plan file
now says:

- **T14** (new) replaces `CreateValueControl`'s `void*` with a typed overload
  set. No string conversion in it. It exists because the current signature
  makes registering a `char[256]` where a `std::string*` is expected a
  compiling, memory-corrupting call — and T5's own note asked for the compiler
  to be on the next person's side before they start. Two of the seven existing
  `TypeString` sites already disagree about whether they pass an array or its
  address, which only a `void*` could hide.
- **T5** is now that coupled change and only that, with the full touch set
  declared — 25 files, reaching into `Species/Camera.cpp`,
  `Species/LocationEditor.cpp` and `Species/Script.cpp`.
- **T15** (new) takes the four window files that were only ever grouped with T5
  because they are Eclipse-derived windows in GameLogic. None registers
  anything `InputField` writes into.
- **T11** gains `ScrollBar`, whose nine sites are all Eclipse widget-name
  copies and would otherwise be converted twice.
- **T8** now `depends_on: T5`. It lands last of the coupled set and owns the
  byte-identity proof, which is what makes that proof cover T5 as well as
  itself. `layering-inversion` T15 has since moved `LevelFile` and
  `GlobalWorld` into GameLogic and emptied `LinkStubs.cpp`, so the round-trip
  is a real test now rather than a review argument — write it that way.
- **T16** (new) is the FilesysUtils remainder T4 recorded as owed: four helpers
  returning pointers into one shared static. It was a sentence in a completed
  task's notes, which is where work goes to be forgotten.
- **T9**'s `depends_on` gained T11, T12, T13 and T15. It is the "sweep the tail
  to zero" task and it could previously have been declared done while three
  grouped tasks were outstanding.

`ownership` T4 and `namespace-migration` T4 gained a `blocked_by` on
`strings-modernised/T15`, since both name `PrefsOtherWindow.cpp`.

`python3 tools/check_task_dag.py` passes on all ten plans.

---

## The restart order

### Step 0 — clear the standing obstacle first: `language-hygiene` T8

`NeuronCore/MathUtils.h` defines function-style `min` and `max` macros, so
`std::min` and `std::max` do not compile anywhere that header is reachable —
which from a NeuronCore header is most of the tree. Every stage 3, 4 and 5
conversion wants those functions, and today each one writes the comparison out
by hand and explains why in a comment. Two completed tasks already carry that
apology.

216 call sites across 52 files, `parallel_safe: false`. It needs the tree to
itself and it is worth having before anything else restarts, because every task
below is a conversion that would otherwise pay the same tax. The hazard is that
the macros double-evaluate their arguments: a call site passing an expression
with a side effect changes behaviour when it stops being evaluated twice. That
is an audit, not a sweep.

### Step 1 — restart the parallel work

These have no path between them and no overlapping files. Four agents, or one
in this order:

| Task | Project | Size | Why now |
|---|---|---|---|
| `strings/T14` | GameLogic | 84 call sites, 11 files | Unblocks T5, which is the critical path. Cheapest thing on it. |
| `strings/T16` | NeuronClient | 4 helpers, 5 callers | Small, tested, and it closes a recorded debt. |
| `language-hygiene/T3` | NeuronCore | 3 enums | Wire values. Pin first, then convert. Independent of everything. |
| `language-hygiene/T4` | NeuronClient | 2 enums, 119 sites, 33 files | Never crosses the wire. Independent. |
| `ownership/T3` | NeuronClient | 8 files | The only stage-5 task that is ready. Sound and GL destructors have side effects — preserve destruction order. |

**Progress, 2026-08-03.** Step 0 and three of step 1 have landed on
`claude/migration-restart-plan-i69so0`: `language-hygiene/T8` (CI green),
`strings/T16` and `strings/T14`. `language-hygiene/T4` was re-scoped before
being started — it claimed three files and five enums, and the five have 795
use sites across 39 files with three of them standing behind `int` typedefs, so
those three are now `language-hygiene/T9`. `language-hygiene/T3` and
`ownership/T3` are untouched.

`strings/T12` (the TextRenderer variadic API, 233 call sites) is also ready and
is `parallel_safe: false`. Run it alone, after step 0 and either before or
after this step — not during. It is the other large exclusive task, and its
value is that `std::format` rejects at compile time what `printf` accepts and
corrupts at run time: **every call site that fails to compile is a latent bug
being surfaced, not a conversion error.** Record them as found.

### Step 2 — the critical path

Everything left funnels through one node:

```
strings/T14 ──▶ strings/T5 ─┬─▶ strings/T8 ──▶ ownership/T5 ─┐
                            ├─▶ strings/T11 ─▶ strings/T13   ├─▶ ownership/T6 ─▶ ownership/T7
                            └─▶ ownership/T4 ────────────────┘        (App)
                                                             └─▶ namespace/T4 ─▶ namespace/T5
```

`strings/T5` gates `strings` T8, T11, T13 and T9; `ownership` T4, T5, T6 and
T7; and `namespace` T4 and T5. Nine of the twenty open tasks are behind it,
directly or transitively. If only one thing is worked at a time, work this.

Two hazards, both recorded on the tasks themselves and both worth repeating
because they fail silently:

- **`CreateValueControl`'s `void*`** — a wrong-typed registration compiles and
  corrupts memory. T14 exists to remove this before T5 starts. Do not skip it
  on the grounds that it looks like tidying.
- **Eclipse looks buttons and windows up by name**, using `strcmp` in some
  places and `stricmp` in others. `std::string::operator==` matches the first
  and not the second, so T11's call sites have to be read rather than swept.
  Getting it wrong breaks a menu button rather than the build.

### Step 3 — stage 5, then the namespaces

`ownership` T4 and T5 unblock as their string tasks land. T6 (App owns its
subsystems) runs last and alone, in small commits, each preserving construction
and destruction order exactly — subsystems reach each other during teardown.
T7 deletes the macros once nothing uses them.

`namespace-migration` is sequenced last on purpose: it is pure churn over files
that should have stopped moving. T3's notes carry two traps that will recur in
T2 and cost a build each — forward declarations of half-converted NeuronCore
types must stay outside the wrapper, and `class Foo*` in a parameter list
*declares* `Neuron::Foo` if it is not already visible. Grep for
`\(class |, class |^\s*class \w+\*` before wrapping anything.

### The owner gate — closed

`rename-darwinian/T4` was blocked on two numbers: 50 Citizens on team 0 in
groups of **30 and 20**, and 179 Virii on team 1 across **eight groups**. The
owner ran the full smoke test at `b0bde71` on 2026-08-03 and reported all seven
steps correct. The plan is closed and `AGENTS.md` records the run.

That is the only evidence the tree has that the rename resolved — CI cannot
produce it, and neither can any agent here. It also re-establishes the baseline
for the restart: every task below starts from a build known to launch, load The
Garden, spawn correctly and advance without tripping the sync assert. **The
next agent to change something reachable from `Location::Advance` is changing
a working game, and the only thing that will say otherwise is another run.**

---

## What has not changed, and what to watch

**Report what you actually ran.** Everything above was measured on Linux with
grep and `check_task_dag.py`. Nothing here was built or launched. The five
Python checks are the only `verify` lines an agent in this environment can
run; the `msbuild` and `vstest` lines are CI's, and the Garden smoke test is
the owner's.

**The recurring failure mode, in the words of the tasks that hit it.** Three
separate completed tasks recorded the same lesson, and it is the one to expect
next:

> The task scoping in this plan was done from grep counts rather than from
> reading call sites, and that keeps producing tasks whose "self-contained"
> premise fails on contact. Expect it; re-scope rather than quietly widening a
> touch set. — `strings/T2`

> Changing a member's type makes the blast radius EVERY mention of that member,
> and I converted the mentions I had noticed. Enumerate every use of anything
> whose type changes, member or not. — `ownership/T1`, `ownership/T2`

T5 is the largest remaining instance of exactly this, which is why it was
re-scoped from measurements before restarting rather than after the third red
build.

**Two findings still outstanding, neither owned by a task.** Both are in
`AGENTS.md` under *Known issues* and both are determinism questions rather than
modernisation ones: `SoundInstance.cpp` draws twice from the simulation's
random stream on paths whose execution depends on sound configuration, and
cross-architecture play has never been tested. Neither blocks any task here.
Neither should be closed by a modernisation commit either.
