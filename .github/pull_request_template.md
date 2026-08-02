## What this changes

<!-- One or two sentences. What is different about the system after this lands? -->

## Why

<!-- The problem, or the task-DAG node this closes (e.g. tasks/neuroncore-layering.yaml T3). -->

## Kind of change

<!-- Tick one. Conversions and behaviour changes belong in separate PRs. -->

- [ ] Modernisation — converting legacy code, no behaviour change intended
- [ ] Layering — removing an upward dependency
- [ ] Fix — behaviour change
- [ ] Build / tooling / docs

## Checks

- [ ] `python3 tools/check_project_files.py`
- [ ] `python3 tools/check_layering.py` — allowlist shrank or is unchanged, never grew
- [ ] `python3 tools/check_task_dag.py`
- [ ] `python3 tools/check_format.py`
- [ ] Builds Debug and Release on at least one platform
- [ ] `vstest.console.exe <Platform>\Debug\*Tests.dll` — the whole suite passes

## Tests

<!--
Which tests cover this change, and where do they live?

New behaviour ships with its tests in the same PR. Anything on the wire, and
anything the simulation depends on being identical everywhere, earns one every
time — see docs/TESTING.md.

If nothing here could be tested, say why. "GameLogic will not link into a test
DLL without a new stub" is a finding worth writing down; an empty section is
not distinguishable from having not thought about it.

If Tests/GameLogicTests/LinkStubs.cpp grew, justify it here. That file may only
shrink.
-->

## Verification

<!--
What did you actually do to check this, and what did you not check?

A green suite is not a running game — it covers wire encoding, string helpers,
object identity and the protocol. The game does run, so the Garden smoke test in
AGENTS.md is available to anyone on Windows; if you could not launch it, say
"it builds and the suite passes; I could not launch the client" rather than
implying more.
-->
