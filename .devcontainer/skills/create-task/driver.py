#!/usr/bin/env python3
"""Driver for the create-task skill.

Creates task notes (Obsidian TaskNotes format) that the handle-task skill
can pick up in a later session, and proves the round trip instead of
assuming it:

  new     scaffold a new note under the repo's TaskNotes/Tasks/ dir
  check   structurally validate the note AND run the handle-task driver's own
          `resolve` + `validate` against it (the exact code a later session
          will use to pick the task up)
  commit  commit just that one note (explicit path — never `git add -A`)

Credential-sensitive work (push / open-mr / finish) stays in the handle-task
driver — this driver never touches tokens. It is read-only with respect to
the remote.

The handle-task driver is located as a sibling skill
(``../handle-task/driver.py`` next to this file) so both skills always agree
on the tasks directory and the frontmatter contract. Override with
``--handle-task-driver`` when the layout differs.

Requires: python3 + pyyaml, git. No network access.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from datetime import date
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("pyyaml is required (pip install pyyaml)")

SKILL_DIR = Path(__file__).resolve().parent
REQUIRED_SECTIONS = ("Goal", "Definition of done", "Files / scope", "Handoff")
SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9-]{1,48}$")
PRIORITY = ("low", "medium", "high")


# ── Location helpers ────────────────────────────────────────────────

def find_repo_root(start: Path | None = None) -> Path:
    cur = (start or Path.cwd()).resolve()
    for cand in [cur, *cur.parents]:
        if (cand / ".git").exists():
            return cand
    sys.exit(f"not inside a git repository (searched up from {cur})")


def find_tasks_dir(root: Path) -> Path:
    for rel in ("porting/TaskNotes/Tasks", "docs/TaskNotes/Tasks"):
        d = root / rel
        if d.is_dir():
            return d
    sys.exit(
        f"no tasks directory under {root}: expected one of "
        f"{[str(root / r) for r in ('porting/TaskNotes/Tasks', 'docs/TaskNotes/Tasks')]}. "
        "Create one before creating tasks."
    )


def find_handle_task_driver(override: str | None) -> Path:
    if override:
        p = Path(override)
        if not p.is_file():
            sys.exit(f"handle-task driver not found at --handle-task-driver path: {p}")
        return p.resolve()
    sibling = SKILL_DIR.parent / "handle-task" / "driver.py"
    if sibling.is_file():
        return sibling
    sys.exit(
        f"could not locate the handle-task driver (looked for {sibling}); "
        "pass --handle-task-driver /path/to/handle-task/driver.py"
    )


def find_task_file(tasks_dir: Path, slug: str) -> Path:
    for cand in (Path(slug), tasks_dir / slug, tasks_dir / f"{slug}.md"):
        if cand.is_file():
            return cand.resolve()
    sys.exit(f"task file not found: {slug} (in {tasks_dir} or as a path)")


# ── Note parsing (mirror of handle-task's contract) ────────────────

def parse_note(path: Path):
    text = path.read_text()
    m = re.match(r"^---\n(.*?)\n---\n(.*)$", text, re.S)
    if not m:
        sys.exit(f"{path}: no YAML frontmatter block found")
    frontmatter = yaml.safe_load(m.group(1)) or {}
    sections: dict[str, str] = {}
    cur, buf = None, []
    for line in m.group(2).splitlines():
        h = re.match(r"^##\s+(.*)$", line)
        if h:
            if cur is not None:
                sections[cur] = "\n".join(buf).strip()
            cur, buf = h.group(1).strip(), []
        elif cur is not None:
            buf.append(line)
    if cur is not None:
        sections[cur] = "\n".join(buf).strip()
    return frontmatter, sections, text


def note_problems(path: Path) -> list[str]:
    """Structural gate: everything handle-task needs to pick the note up."""
    try:
        fm, sections, _ = parse_note(path)
    except SystemExit as e:
        return [str(e.code)]

    problems = []
    for key in ("title", "repo", "status", "priority", "dateCreated"):
        if not fm.get(key):
            problems.append(f"frontmatter missing '{key}:'")
    if fm.get("status") not in (None, "open", "done"):
        problems.append(f"frontmatter 'status' must be open|done, got {fm.get('status')!r}")
    if fm.get("priority") not in PRIORITY:
        problems.append(f"frontmatter 'priority' must be one of {PRIORITY}, got {fm.get('priority')!r}")
    tags = fm.get("tags") or []
    if "task" not in tags:
        problems.append("frontmatter 'tags' must include 'task' (TaskNotes indexing)")
    dc = str(fm.get("dateCreated") or "")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", dc):
        problems.append(f"frontmatter 'dateCreated' must be YYYY-MM-DD, got {dc!r}")

    for sec in REQUIRED_SECTIONS:
        if sec not in sections or not sections[sec].strip():
            problems.append(f"missing or empty '## {sec}' section")

    dod = sections.get("Definition of done", "")
    if not re.search(r"^- \[ \]", dod, re.M):
        problems.append("'## Definition of done' has no unchecked '- [ ]' checklist items")

    handoff = sections.get("Handoff", "")
    if not re.search(r"branch `([^`]+)`", handoff):
        problems.append("'## Handoff' must contain a backticked branch line: `branch <name>`")

    # Active (unstruck) blockers are legal at creation time — they only stop
    # open-mr later — so report, don't fail.
    for line in sections.get("Blocked by", "").splitlines():
        line = line.strip()
        if line.startswith("-"):
            content = line[1:].strip()
            if not (content.startswith("~~") and content.endswith("~~")):
                problems.append(f"NOTE: active blocker in '## Blocked by': {content} "
                                "(handle-task will refuse open-mr until it is struck through)")

    # Placeholder gate: `new` seeds sections with TODO: lines; none may remain.
    for m in re.finditer(r"^.+TODO:.+$", path.read_text(), re.M):
        problems.append(f"unfilled placeholder: {m.group(0).strip()}")
    return problems


# ── Rendering ───────────────────────────────────────────────────────

def render_note(slug: str, title: str, repo: str, priority: str,
                 target: str, branch: str, blocked_by: list[str],
                 notes: list[str]) -> str:
    today = date.today().isoformat()
    # NOTE: the no-blocker placeholder must be exactly `~~none~~` — handle-task
    # only skips blocker lines that both start and end with `~~`, and a
    # non-struck line makes `open-mr` refuse to run.
    blocked_lines = "\n".join(f"- {b}" for b in blocked_by) if blocked_by else "- ~~none~~"
    note_lines = "\n".join(f"- {n}" for n in notes) if notes else "TODO: add environment hazards, reference material, and iteration tips here."
    return f"""---
title: {title}
status: open             # open | done (driver rewrites this on `finish`)
priority: {priority}          # low | medium | high
repo: {repo}          # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []              # e.g. dev, cockpit, gpu1
projects: []              # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0           # minutes
dateCreated: {today}
# dateModified / completedDate are added automatically by `driver.py finish`
---
## Goal
TODO: one sentence — the outcome, and the state that counts as "done".

## Approach
TODO: ordered phases with concrete commands/paths, in strict order. This section
guides the planning step of `handle-task`; keep every step executable.

## Definition of done
- [ ] TODO: first verifiable deliverable
- [ ] ...
- [ ] MR/PR opened into `{target}` and linked below

## Files / scope
TODO: explicit paths to touch. End with the exclusions: "Do **not** change ...".

## Notes
{note_lines}

## Blocked by
{blocked_lines}

## Progress log

## Handoff
- branch `{branch}`
- TODO: MR/PR line is written by `driver.py finish` (`- {{PR|MR}}: <label> {{#|!}}<iid> <url>`)
Refs: TODO: paths to the docs that back this task
"""


# ── Commands ────────────────────────────────────────────────────────

def cmd_new(args):
    root = find_repo_root(Path(args.repo_root) if args.repo_root else None)
    tasks_dir = find_tasks_dir(root)

    if not SLUG_RE.fullmatch(args.slug):
        sys.exit(f"invalid slug {args.slug!r}: use kebab-case, 2-49 chars, [a-z0-9-]")
    path = tasks_dir / f"{args.slug}.md"
    if path.exists():
        sys.exit(f"task note already exists: {path} (edit it in place instead of re-creating)")

    if not args.repo:
        keys = sorted({re.match(r"^(GITHUB|GITLAB)_(.*)_TOKEN$", k).group(2)
                       for k in os.environ if re.match(r"^(GITHUB|GITLAB)_(.*)_TOKEN$", k)})
        sys.exit("missing --repo: pass a repo key matching a token env var. "
                 f"Available keys: {keys or '(none set)'}")
    up = args.repo.upper()
    if not (os.environ.get(f"GITHUB_{up}_TOKEN") or os.environ.get(f"GITLAB_{up}_TOKEN")):
        print(f"WARNING: no GITHUB_{up}_TOKEN / GITLAB_{up}_TOKEN in the environment. "
              f"The note is still valid; handle-task will ask for it at pickup time.",
              file=sys.stderr)

    prefix = os.environ.get("AGENT_BRANCH_PREFIX")
    if not prefix:
        sys.exit("missing AGENT_BRANCH_PREFIX in environment (needed for the Handoff branch line)")
    target = args.target or os.environ.get("TARGET_BRANCH", "development")

    title = args.title or re.sub(r"-", " ", args.slug).title()
    text = render_note(args.slug, title, args.repo, args.priority, target,
                       f"{prefix}/{args.slug}", args.blocked_by, args.note)
    path.write_text(text)

    print(json.dumps({
        "note": str(path),
        "slug": args.slug,
        "repo": args.repo,
        "target_branch": target,
        "branch": f"{prefix}/{args.slug}",
        "next": "fill every TODO: section, then run: "
                "driver.py check <slug> && driver.py commit <slug>",
    }, indent=2))


def cmd_check(args):
    root = find_repo_root(Path(args.repo_root) if args.repo_root else None)
    tasks_dir = find_tasks_dir(root)
    path = find_task_file(tasks_dir, args.slug)
    ht = find_handle_task_driver(args.handle_task_driver)

    problems = note_problems(path)
    hard = [p for p in problems if not p.startswith("NOTE:")]
    info = [p for p in problems if p.startswith("NOTE:")]

    # Round trip: run the handle-task driver's own resolve/validate on this note.
    def run_ht(*sub: str):
        r = subprocess.run([sys.executable, str(ht), *sub],
                           capture_output=True, text=True, timeout=60)
        return r

    resolve_json, resolve_note = None, ""
    r = run_ht("resolve", str(path))
    if r.returncode == 0:
        try:
            resolve_json = json.loads(r.stdout)
        except json.JSONDecodeError:
            resolve_note = f"resolve printed non-JSON: {r.stdout.strip()}"
    else:
        resolve_note = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else "resolve failed"
        # A missing token is a legitimate, fixable state at creation time
        # (handle-task asks for it at pickup). Surface it as a warning, not a
        # hard failure. Anything else from resolve is a real problem.
        if "no token for repo" in resolve_note:
            info.append(f"NOTE: {resolve_note} (structure is fine; set the token before pickup)")
            resolve_note = ""

    # The stale handle-task driver has no `validate` subcommand; degrade
    # gracefully instead of recording a usage error as a finding.
    has_validate = subprocess.run([sys.executable, str(ht), "--help"],
                                  capture_output=True, text=True).stdout.find("validate") != -1
    if has_validate:
        v = run_ht("validate")
        validate_note = (v.stdout + v.stderr).strip()
    else:
        validate_note = "handle-task driver has no `validate` subcommand (structural check above covers this note)"

    ok = not hard
    print(json.dumps({
        "note": str(path),
        "slug": args.slug,
        "ok": ok,
        "errors": hard,
        "warnings": info,
        "handle_task_driver": str(ht),
        "handle_task_resolve": resolve_json,
        "handle_task_resolve_note": resolve_note,
        "handle_task_validate": validate_note,
    }, indent=2))
    if not ok:
        sys.exit(f"check failed: {len(hard)} problem(s) — fix before the task is schedulable")


def cmd_commit(args):
    root = find_repo_root(Path(args.repo_root) if args.repo_root else None)
    tasks_dir = find_tasks_dir(root)
    path = find_task_file(tasks_dir, args.slug)

    problems = [p for p in note_problems(path) if not p.startswith("NOTE:")]
    if problems and not args.force:
        sys.exit(f"refusing to commit a note that does not pass check:\n  "
                 + "\n  ".join(problems) + "\n(use --force to commit anyway)")

    rel = path.relative_to(root)
    for step in (
        ["git", "add", str(rel)],
        ["git", "commit", "-m", f"Add task note {args.slug}"],
    ):
        r = subprocess.run(step, cwd=root, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f"{' '.join(step[:2])} failed: {r.stdout.strip() or r.stderr.strip()}")

    sha = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=root,
                         capture_output=True, text=True).stdout.strip()
    branch = subprocess.run(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=root,
                            capture_output=True, text=True).stdout.strip()
    print(json.dumps({"committed": str(rel), "sha": sha, "branch": branch,
                      "note": "push (if desired) via the handle-task driver on a feature branch"}, indent=2))


# ── Argument parsing ────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("new", help="scaffold a new task note")
    p.add_argument("slug", help="kebab-case slug, e.g. llvm-14-port or fix-bbnum-ids")
    p.add_argument("--title", help="title (default: slug with spaces)")
    p.add_argument("--repo", help="repo key matching a GITHUB_/GITLAB_ <KEY>_TOKEN")
    p.add_argument("--priority", default="medium", choices=PRIORITY)
    p.add_argument("--target", help="target branch for the eventual MR/PR "
                                    "(default: $TARGET_BRANCH or development)")
    p.add_argument("--blocked-by", action="append", default=[],
                   help="blocking task slug (repeatable)")
    p.add_argument("--note", action="append", default=[],
                   help="seed line for the Notes section (repeatable)")
    p.add_argument("--repo-root", help="explicit repo root (default: detect from cwd)")
    p.set_defaults(func=cmd_new)

    for name, h in (("check", "validate the note + prove handle-task can resolve it"),
                    ("commit", "commit just this note (explicit path)")):
        p = sub.add_parser(name, help=h)
        p.add_argument("slug")
        p.add_argument("--handle-task-driver", help="explicit path to handle-task/driver.py")
        p.add_argument("--repo-root", help="explicit repo root (default: detect from cwd)")
        if name == "commit":
            p.add_argument("--force", action="store_true", help="commit even if check fails")
        p.set_defaults(func=cmd_check if name == "check" else cmd_commit)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
