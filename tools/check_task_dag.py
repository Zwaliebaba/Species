#!/usr/bin/env python3
"""Validate and query task DAGs under tasks/.

A plan is a set of tasks plus the edges between them. The graph must be acyclic,
every dependency must resolve, and status must be consistent with the edges — a
task cannot be done while something it depends on is not.

The point of the format is that an agent can compute what to do next instead of
being told. `--next` lists tasks whose dependencies are all satisfied; `--waves`
shows which tasks may run concurrently.

    python3 tools/check_task_dag.py                       # validate every plan
    python3 tools/check_task_dag.py tasks/foo.yaml
    python3 tools/check_task_dag.py --next tasks/foo.yaml
    python3 tools/check_task_dag.py --waves tasks/foo.yaml
    python3 tools/check_task_dag.py --mermaid tasks/foo.yaml

The schema is documented in docs/TASK_DAG.md.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("PyYAML is required:  pip install pyyaml")

REPO_ROOT = Path(__file__).resolve().parent.parent
TASKS_DIR = REPO_ROOT / "tasks"

REQUIRED_PLAN_KEYS = {"plan", "title", "tasks"}
REQUIRED_TASK_KEYS = {"id", "title", "intent", "acceptance", "status"}
OPTIONAL_TASK_KEYS = {
    "depends_on",
    "project",
    "files",
    "verify",
    "notes",
    "phase",
    "parallel_safe",
}
STATUSES = {"todo", "in_progress", "blocked", "done", "abandoned"}

# A task in one of these states counts as satisfied for its dependents.
SATISFIED = {"done", "abandoned"}


class PlanError(Exception):
    pass


def display(path: Path) -> str:
    """Repo-relative where possible; plans may also be validated from elsewhere."""
    try:
        return str(path.resolve().relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def load_plan(path: Path) -> dict:
    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise PlanError("plan must be a YAML mapping")

    missing = REQUIRED_PLAN_KEYS - data.keys()
    if missing:
        raise PlanError(f"plan is missing required key(s): {', '.join(sorted(missing))}")
    if not isinstance(data["tasks"], list) or not data["tasks"]:
        raise PlanError("'tasks' must be a non-empty list")
    return data


def validate_tasks(tasks: list) -> dict[str, dict]:
    by_id: dict[str, dict] = {}
    for index, task in enumerate(tasks):
        if not isinstance(task, dict):
            raise PlanError(f"task #{index} is not a mapping")

        missing = REQUIRED_TASK_KEYS - task.keys()
        if missing:
            raise PlanError(f"task #{index} is missing key(s): {', '.join(sorted(missing))}")

        unknown = task.keys() - REQUIRED_TASK_KEYS - OPTIONAL_TASK_KEYS
        if unknown:
            raise PlanError(f"task {task['id']}: unknown key(s): {', '.join(sorted(unknown))}")

        task_id = task["id"]
        if task_id in by_id:
            raise PlanError(f"duplicate task id: {task_id}")
        if task["status"] not in STATUSES:
            raise PlanError(
                f"task {task_id}: status '{task['status']}' is not one of {sorted(STATUSES)}"
            )
        if not task["acceptance"]:
            raise PlanError(f"task {task_id}: 'acceptance' must list at least one criterion")

        by_id[task_id] = task

    for task_id, task in by_id.items():
        for dependency in task.get("depends_on") or []:
            if dependency not in by_id:
                raise PlanError(f"task {task_id} depends on unknown task '{dependency}'")
            if dependency == task_id:
                raise PlanError(f"task {task_id} depends on itself")

    return by_id


def topological_waves(by_id: dict[str, dict]) -> list[list[str]]:
    """Group tasks into waves; every task in a wave may run concurrently.

    Raises PlanError naming the tasks involved if the graph contains a cycle.
    """
    remaining = {
        task_id: set(task.get("depends_on") or []) for task_id, task in by_id.items()
    }
    waves: list[list[str]] = []
    resolved: set[str] = set()

    while remaining:
        ready = sorted(t for t, deps in remaining.items() if deps <= resolved)
        if not ready:
            raise PlanError(
                "dependency cycle among: " + ", ".join(sorted(remaining))
            )
        waves.append(ready)
        resolved.update(ready)
        for task_id in ready:
            del remaining[task_id]

    return waves


def validate_status_consistency(by_id: dict[str, dict]) -> list[str]:
    problems = []
    for task_id, task in by_id.items():
        if task["status"] not in {"done", "in_progress"}:
            continue
        for dependency in task.get("depends_on") or []:
            if by_id[dependency]["status"] not in SATISFIED:
                problems.append(
                    f"task {task_id} is '{task['status']}' but depends on {dependency}, "
                    f"which is '{by_id[dependency]['status']}'"
                )
    return problems


def ready_tasks(by_id: dict[str, dict]) -> list[str]:
    return sorted(
        task_id
        for task_id, task in by_id.items()
        if task["status"] == "todo"
        and all(by_id[d]["status"] in SATISFIED for d in task.get("depends_on") or [])
    )


def to_mermaid(plan: dict, by_id: dict[str, dict]) -> str:
    shape = {
        "done": '["{label}"]',
        "abandoned": '["{label}"]',
        "in_progress": '("{label}")',
        "blocked": '{{"{label}"}}',
        "todo": '["{label}"]',
    }
    lines = ["```mermaid", "graph TD"]
    for task_id, task in by_id.items():
        label = f"{task_id}: {task['title']}".replace('"', "'")
        lines.append("  " + task_id + shape[task["status"]].format(label=label))
    for task_id, task in by_id.items():
        for dependency in task.get("depends_on") or []:
            lines.append(f"  {dependency} --> {task_id}")
    for status, style in (
        ("done", "fill:#d4edda,stroke:#28a745"),
        ("in_progress", "fill:#fff3cd,stroke:#ffc107"),
        ("blocked", "fill:#f8d7da,stroke:#dc3545"),
    ):
        for task_id, task in by_id.items():
            if task["status"] == status:
                lines.append(f"  style {task_id} {style}")
    lines.append("```")
    return "\n".join(lines)


def process(path: Path, args: argparse.Namespace) -> bool:
    try:
        plan = load_plan(path)
        by_id = validate_tasks(plan["tasks"])
        waves = topological_waves(by_id)
    except (PlanError, yaml.YAMLError) as error:
        print(f"{display(path)}: {error}")
        return False

    problems = validate_status_consistency(by_id)
    if problems:
        print(f"{display(path)}: inconsistent status:")
        for problem in problems:
            print(f"  {problem}")
        return False

    if args.mermaid:
        print(to_mermaid(plan, by_id))
        return True

    if args.waves:
        print(f"{plan['plan']}: {plan['title']}")
        for index, wave in enumerate(waves, start=1):
            print(f"\n  wave {index} ({len(wave)} task(s), may run concurrently):")
            for task_id in wave:
                task = by_id[task_id]
                print(f"    [{task['status']:11s}] {task_id}  {task['title']}")
        return True

    if args.next:
        ready = ready_tasks(by_id)
        if not ready:
            unfinished = [t for t, v in by_id.items() if v["status"] not in SATISFIED]
            print("no task is ready" + (" — plan complete" if not unfinished else ""))
            return True
        print(f"{plan['plan']}: ready to start")
        for task_id in ready:
            print(f"  {task_id}  {by_id[task_id]['title']}")
        return True

    done = sum(1 for t in by_id.values() if t["status"] in SATISFIED)
    print(
        f"{display(path)}: OK — {len(by_id)} tasks, "
        f"{len(waves)} wave(s), {done}/{len(by_id)} complete"
    )
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plans", nargs="*", type=Path, help="plan files (default: tasks/*.yaml)")
    parser.add_argument("--next", action="store_true", help="list tasks ready to start")
    parser.add_argument("--waves", action="store_true", help="show concurrent execution waves")
    parser.add_argument("--mermaid", action="store_true", help="emit a Mermaid graph")
    args = parser.parse_args()

    paths = args.plans or sorted(TASKS_DIR.glob("*.yaml"))
    if not paths:
        print("no task plans found under tasks/")
        return 0

    return 0 if all([process(path, args) for path in paths]) else 1


if __name__ == "__main__":
    sys.exit(main())
