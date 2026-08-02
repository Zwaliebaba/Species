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

## Verification

<!--
What did you actually do to check this, and what did you not check?

The game does not currently run, so "it compiles" is often the honest ceiling.
Say that rather than implying more.
-->
