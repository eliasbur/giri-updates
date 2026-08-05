#!/usr/bin/env python3
"""Driver for the handle-task skill.

Handles the mechanical, credential-sensitive parts of working a
porting/TaskNotes/Tasks/*.md note: figuring out which repo it belongs to,
pushing a branch over HTTPS with the right token, opening a merge
request or pull request via the appropriate CLI (glab or gh), and
writing the Handoff/status fields back into the task note.

Backend per repo is determined by whichever token env var is set:
  GITHUB_<NAME>_TOKEN  ->  gh  (github.com)
  GITLAB_<NAME>_TOKEN   ->  glab (git.tu-berlin.de)

Requires at least one *_<NAME>_TOKEN and *_<NAME>_PATH in the
environment (`set -a; source .env; set +a` from the repo root), plus
AGENT_BRANCH_PREFIX.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
from datetime import datetime, timedelta, timezone
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[3]
ASKPASS_SCRIPT = Path(__file__).resolve().parent / "askpass.sh"
TASKS_DIR = REPO_ROOT / "porting" / "TaskNotes" / "Tasks"
BERLIN = timezone(timedelta(hours=2))


# ── Repo config ────────────────────────────────────────────────────

def get_repo_cfg(repo_key: str) -> dict:
    """Build repo config from env vars. Fatal error if missing."""
    up = repo_key.upper()

    gh_token = os.environ.get(f"GITHUB_{up}_TOKEN")
    gl_token = os.environ.get(f"GITLAB_{up}_TOKEN")

    if not gh_token and not gl_token:
        sys.exit(
            f"no token for repo '{repo_key}': "
            f"set either GITHUB_{up}_TOKEN or GITLAB_{up}_TOKEN"
        )

    if gh_token:
        backend = "github"
        host = "github.com"
        cli_tool = "gh"
        cli_env = "GH_TOKEN"
        token_env = f"GITHUB_{up}_TOKEN"
        token = gh_token
    else:
        backend = "gitlab"
        host = "git.tu-berlin.de"
        cli_tool = "glab"
        cli_env = "GITLAB_TOKEN"
        token_env = f"GITLAB_{up}_TOKEN"
        token = gl_token

    path = os.environ.get(f"{backend.upper()}_{up}_PATH")
    if not path:
        sys.exit(f"missing {backend.upper()}_{up}_PATH for repo '{repo_key}'")

    # Local path
    local_env = f"{backend.upper()}_{up}_LOCAL_PATH"
    local_path_str = os.environ.get(local_env, ".")
    local_root = REPO_ROOT / local_path_str if local_path_str != "." else REPO_ROOT

    cfg = {
        "repo_key": repo_key,
        "backend": backend,
        "host": host,
        "path": path,
        "local_root": local_root,
        "token": token,
        "token_env": token_env,
        "cli_tool": cli_tool,
        "cli_env": cli_env,
    }

    # Terminology
    if backend == "gitlab":
        cfg["term_mr"] = "MR"
        cfg["term_prefix"] = "!"
    else:
        cfg["term_mr"] = "PR"
        cfg["term_prefix"] = "#"

    return cfg


# ── Task parsing ───────────────────────────────────────────────────

def find_task_file(arg: str) -> Path:
    p = Path(arg)
    candidates = [p, TASKS_DIR / arg, TASKS_DIR / f"{arg}.md"]
    for c in candidates:
        if c.is_file():
            return c.resolve()
    sys.exit(f"task file not found: {arg} (tried {[str(c) for c in candidates]})")


def parse_task(path: Path):
    text = path.read_text()
    m = re.match(r"^---\n(.*?)\n---\n(.*)$", text, re.S)
    if not m:
        sys.exit(f"{path}: no YAML frontmatter block found")
    frontmatter = yaml.safe_load(m.group(1)) or {}
    sections: dict[str, str] = {}
    cur = None
    buf: list[str] = []
    for line in m.group(2).splitlines():
        h = re.match(r"^##\s+(.*)$", line)
        if h:
            if cur is not None:
                sections[cur] = "\n".join(buf).strip()
            cur = h.group(1).strip()
            buf = []
        elif cur is not None:
            buf.append(line)
    if cur is not None:
        sections[cur] = "\n".join(buf).strip()
    title = frontmatter.get("title") or path.stem.replace("-", " ")
    return frontmatter, sections, title, text


def resolve_repo_key(frontmatter: dict) -> str:
    """Read repo key from task frontmatter. Error if missing."""
    key = frontmatter.get("repo")
    if not key:
        sys.exit("task note missing 'repo:' in YAML frontmatter")
    key = str(key)
    # Sanity: token must exist
    up = key.upper()
    _ = os.environ.get(f"GITHUB_{up}_TOKEN") or os.environ.get(f"GITLAB_{up}_TOKEN")
    return key


def resolve_branch(frontmatter: dict, path: Path, sections: dict | None = None) -> str:
    """Build branch name from AGENT_BRANCH_PREFIX + task slug."""
    prefix = os.environ.get("AGENT_BRANCH_PREFIX")
    if not prefix:
        sys.exit("missing AGENT_BRANCH_PREFIX in environment")
    handoff = ""
    if sections is None:
        _, sections, _, _ = parse_task(path)
    handoff = sections.get("Handoff", "")
    m = re.search(r"branch `([^`]+)`", handoff)
    if m:
        return m.group(1)
    return f"{prefix}/{path.stem}"


def resolve_target_branch() -> str:
    return os.environ.get("TARGET_BRANCH", "development")


def unresolved_blockers(sections: dict) -> list[str]:
    out = []
    for line in sections.get("Blocked by", "").splitlines():
        line = line.strip()
        if not line.startswith("-"):
            continue
        content = line[1:].strip()
        if content.startswith("~~") and content.endswith("~~"):
            continue
        out.append(content)
    return out


# ── Git helpers ─────────────────────────────────────────────────────

def run_git(
    repo_root: Path, *args: str, env_extra: dict | None = None, timeout: int = 45
) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    if env_extra:
        env.update(env_extra)
    try:
        return subprocess.run(
            ["git", *args], cwd=repo_root, env=env,
            capture_output=True, text=True, timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(
            args, 124, "", f"git {' '.join(args)} timed out after {timeout}s"
        )


def current_branch(repo_root: Path) -> str:
    r = run_git(repo_root, "rev-parse", "--abbrev-ref", "HEAD")
    if r.returncode != 0:
        sys.exit(f"could not determine current branch in {repo_root}: {r.stderr}")
    return r.stdout.strip()


def get_protected_branches() -> tuple:
    target = resolve_target_branch()
    return (target, "main")


# ── Push ────────────────────────────────────────────────────────────

def push_branch(cfg: dict, branch: str, retries: int, backoff: int, force: bool = False):
    protected = get_protected_branches()
    if branch in protected:
        sys.exit(
            f"refusing to push directly to {branch!r} \u2014 every change, including "
            f"task-note bookkeeping, must land via a {cfg['term_mr']}, never a direct "
            "push to a protected branch. Commit to a feature branch and push/open "
            "one for that instead."
        )
    token = cfg["token"]
    url = f"https://{cfg['host']}/{cfg['path']}.git"
    env_extra = {
        "GIT_ASKPASS": str(ASKPASS_SCRIPT),
        "GIT_ASKPASS_TOKEN": token,
        "GIT_TERMINAL_PROMPT": "0",
    }
    push_args = ["push", url, f"{branch}:{branch}"]
    if force:
        push_args.insert(1, "--force")
    last = ""
    for attempt in range(1, retries + 1):
        r = run_git(cfg["local_root"], *push_args, env_extra=env_extra)
        if r.returncode == 0:
            print(r.stderr.strip())
            return
        last = r.stderr.strip()
        tail = last.splitlines()[-1] if last else "unknown error"
        print(f"[push] attempt {attempt}/{retries} failed: {tail}", file=sys.stderr)
        if attempt < retries:
            time.sleep(backoff)
    sys.exit(f"push failed after {retries} attempts:\n{last}")


# ── CLI tool wrappers ──────────────────────────────────────────────

def _run_cli(
    cfg: dict, *args: str, env_extra: dict | None = None, timeout: int = 60
) -> subprocess.CompletedProcess:
    tool = cfg["cli_tool"]
    env = os.environ.copy()
    if env_extra:
        env.update(env_extra)
    try:
        return subprocess.run(
            [tool, *args], env=env, capture_output=True, text=True, timeout=timeout
        )
    except FileNotFoundError as e:
        return subprocess.CompletedProcess(args, 127, "", str(e))
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(args, 124, "", f"{tool} {' '.join(args)} timed out")


def _cli_mr_create(
    cfg: dict,
    branch: str,
    target: str,
    title: str,
    description: str,
    retries: int,
    backoff: int,
) -> dict:
    """Open MR/PR via CLI (glab or gh). Returns result dict or raises."""
    token = cfg["token"]
    cli_env = cfg["cli_env"]
    env_extra = {cli_env: token}
    last_err = ""

    for attempt in range(1, retries + 1):
        result, proc = _try_mr_create(cfg, branch, target, title, description, env_extra)
        if result is not None:
            print(f"[{cfg['cli_tool']}] {cfg['term_mr']} opened (attempt {attempt}/{retries})",
                  file=sys.stderr)
            return result

        last_err = proc.stderr.strip() if proc.returncode else "unknown error"
        tail = last_err.splitlines()[-1] if last_err else "unknown error"
        print(f"[{cfg['cli_tool']} mr create] attempt {attempt}/{retries} failed: {tail}",
              file=sys.stderr)
        if attempt < retries:
            time.sleep(backoff)

    sys.exit(f"{cfg['cli_tool']} failed after {retries} attempts:\n{last_err}")


def _try_mr_create(cfg: dict, branch: str, target: str, title: str,
                   description: str, env_extra: dict) -> tuple[dict | None, subprocess.CompletedProcess]:
    """Try one MR/PR creation attempt. Returns (result_dict, proc) on success, (None, proc) on failure."""
    assignee = os.environ.get("ASSIGNEE", "").strip()
    glab_args = [
        "-R", cfg["path"],
        "mr", "create",
        "--push", "--yes",
        "-s", branch,
        "-b", target,
        "-t", title,
        "-d", description,
    ]
    if assignee:
        glab_args.extend(["--assignee", assignee])

    gh_args = [
        "-R", cfg["path"],
        "pr", "create",
        "--head", branch,
        "--base", target,
        "-t", title,
        "-b", description,
    ]
    if assignee:
        gh_args.extend(["--assignee", assignee])

    if cfg["backend"] == "gitlab":
        r = _run_cli(cfg, *glab_args, env_extra=env_extra, timeout=120)
    else:
        r = _run_cli(cfg, *gh_args, env_extra=env_extra, timeout=120)

    if r.returncode != 0:
        return None, r

    return _parse_mr_result(cfg, r.stdout), r


def _parse_mr_result(cfg: dict, stdout: str) -> dict:
    """Parse CLI output into normalized MR/PR result dict."""
    # Try to extract URL and number
    url_match = re.search(r"(https?://[^\s]+)", stdout)
    web_url = url_match.group(1) if url_match else ""

    # Extract number from URL or output
    num_match = re.search(rf"merge_requests/(\d+)|(?:pulls|pr)(?:/|#)(\d+)", web_url)
    if not num_match:
        num_match = re.search(rf"[{cfg['term_prefix']}]?(\d+)", stdout)
    number = int(num_match.group(1) or num_match.group(2) or 0) if num_match else 0

    return {
        "repo": cfg["repo_key"],
        "backend": cfg["backend"],
        "type": cfg["term_mr"],
        "number": number,
        "web_url": web_url,
    }


# ── Commands ────────────────────────────────────────────────────────

def cmd_resolve(args):
    path = find_task_file(args.task)
    frontmatter, sections, title, _ = parse_task(path)
    repo_key = resolve_repo_key(frontmatter)
    cfg = get_repo_cfg(repo_key)
    branch = resolve_branch(frontmatter, path, sections)
    target = resolve_target_branch()
    blockers = unresolved_blockers(sections)

    print(json.dumps({
        "task_file": str(path),
        "title": title,
        "status": frontmatter.get("status"),
        "repo": repo_key,
        "backend": cfg["backend"],
        "term_mr": cfg["term_mr"],
        "term_prefix": cfg["term_prefix"],
        "repo_root": str(cfg["local_root"]),
        "target_branch": target,
        "branch": branch,
        "blocked_by": blockers,
    }, indent=2))


def cmd_push(args):
    path = find_task_file(args.task)
    frontmatter, sections, _, _ = parse_task(path)
    repo_key = resolve_repo_key(frontmatter)
    cfg = get_repo_cfg(repo_key)
    branch = resolve_branch(frontmatter, path, sections)
    push_branch(cfg, branch, args.retries, args.backoff, force=args.force)


def cmd_open_mr(args):
    path = find_task_file(args.task)
    frontmatter, sections, title, _ = parse_task(path)
    repo_key = resolve_repo_key(frontmatter)
    cfg = get_repo_cfg(repo_key)

    # --no-glab / --no-gh validation
    if cfg["backend"] == "gitlab" and args.no_glab:
        sys.exit(f"refusing to open {cfg['term_mr']}: --no-glab is set but backend is GitLab")
    if cfg["backend"] == "github" and args.no_gh:
        sys.exit(f"refusing to open {cfg['term_mr']}: --no-gh is set but backend is GitHub")

    blockers = unresolved_blockers(sections)
    if blockers and not args.force:
        sys.exit(
            "unresolved blockers, refusing to open "
            f"{cfg['term_mr']} (use --force to override):\n"
            + "\n".join(f"  - {b}" for b in blockers)
        )

    branch = resolve_branch(frontmatter, path, sections)
    target = args.target if args.target else resolve_target_branch()
    mr_title = args.title or title
    mr_desc = args.description or f"Implements {path.relative_to(REPO_ROOT)}."

    result = _cli_mr_create(cfg, branch, target, mr_title, mr_desc, args.retries, args.backoff)
    print(json.dumps(result, indent=2))


def cmd_close_mr(args):
    cfg = get_repo_cfg(args.repo)
    if cfg["backend"] == "gitlab" and args.no_glab:
        sys.exit(f"refusing to close: --no-glab is set but backend is GitLab")
    if cfg["backend"] == "github" and args.no_gh:
        sys.exit(f"refusing to close: --no-gh is set but backend is GitHub")
    iid = args.iid
    token = cfg["token"]
    env_extra = {cfg["cli_env"]: token}

    r = _try_close(cfg, iid, env_extra)
    if r is not None:
        print(json.dumps({
            "repo": cfg["repo_key"],
            "backend": cfg["backend"],
            "term_mr": cfg["term_mr"],
            "number": iid,
            "state": "closed",
        }, indent=2))
        return

    sys.exit(f"{cfg['cli_tool']} close-mr failed:\n{r.stderr if r else 'unknown error'}")


def _try_close(cfg: dict, iid: int, env_extra: dict) -> subprocess.CompletedProcess | None:
    if cfg["backend"] == "gitlab":
        r = _run_cli(cfg,
                      "-R", cfg["path"],
                      "mr", "close", str(iid),
                      env_extra=env_extra, timeout=60)
    else:
        r = _run_cli(cfg,
                      "pr", "close", str(iid),
                      "--repo", cfg["path"],
                      env_extra=env_extra, timeout=60)

    if r.returncode != 0:
        return None
    return r


def cmd_delete_branch(args):
    cfg = get_repo_cfg(args.repo)
    if cfg["backend"] == "gitlab" and args.no_glab:
        sys.exit(f"refusing to delete branch: --no-glab is set but backend is GitLab")
    if cfg["backend"] == "github" and args.no_gh:
        sys.exit(f"refusing to delete branch: --no-gh is set but backend is GitHub")
    token = cfg["token"]
    env_extra = {cfg["cli_env"]: token}

    r = _try_delete_branch(cfg, args.branch, env_extra)
    if r is not None:
        print(f"deleted {args.branch} on {cfg['repo_key']}")
        return

    sys.exit(f"{cfg['cli_tool']} delete-branch failed:\n{r.stderr if r else 'unknown error'}")


def _try_delete_branch(cfg: dict, branch: str, env_extra: dict) -> subprocess.CompletedProcess | None:
    encoded = urllib.parse.quote(branch, safe="")
    if cfg["backend"] == "gitlab":
        r = _run_cli(cfg,
                      "-R", cfg["path"],
                      "api", "-X", "DELETE", f"/repository/branches/{encoded}",
                      env_extra=env_extra, timeout=60)
    else:
        r = _run_cli(cfg,
                      "api", "-X", "DELETE", f"/repos/{cfg['path']}/git/refs/heads/{encoded}",
                      env_extra=env_extra, timeout=60)

    if r.returncode != 0:
        return None
    return r


def cmd_finish(args):
    path = find_task_file(args.task)
    frontmatter, sections, title, text = parse_task(path)
    repo_key = resolve_repo_key(frontmatter)
    cfg = get_repo_cfg(repo_key)

    m = re.match(r"^(---\n)(.*?)(\n---\n)(.*)$", text, re.S)
    fm_text = m.group(2)
    body = m.group(4)
    now = datetime.now(BERLIN)
    date_modified = now.strftime("%Y-%m-%dT%H:%M:%S.") + f"{now.microsecond // 1000:03d}+02:00"
    today = now.strftime("%Y-%m-%d")

    # Update status in frontmatter
    if re.search(r"^status:.*$", fm_text, re.M):
        fm_text = re.sub(r"^status:.*$", "status: done", fm_text, count=1, flags=re.M)
    else:
        fm_text += "\nstatus: done"

    # Update dateModified in frontmatter
    if re.search(r"^dateModified:.*$", fm_text, re.M):
        fm_text = re.sub(
            r"^dateModified:.*$",
            f"dateModified: {date_modified}",
            fm_text, count=1, flags=re.M
        )
    else:
        fm_text += f"\ndateModified: {date_modified}"

    # completedDate
    if "completedDate" not in frontmatter:
        fm_text += f"\ncompletedDate: {today}"

    # Handoff line
    prefix = cfg["term_prefix"]
    term = cfg["term_mr"]
    handoff_line = f"- {term}: {args.label} {prefix}{args.iid} {args.url}"

    body = m.group(4)
    if "## Handoff" in body:
        if re.search(rf"^- (?:{term}|MR|PR):.*$", body, re.M):
            body = re.sub(rf"^- (?:{term}|MR|PR):.*$", handoff_line, body, count=1, flags=re.M)
        else:
            body = re.sub(r"(## Handoff\n)", r"\1" + handoff_line + "\n", body, count=1)
    else:
        body += f"\n\n## Handoff\n{handoff_line}\n"

    new_text = f"---\n{fm_text}\n---\n{body}"
    path.write_text(new_text)

    if not args.no_commit:
        rel = path.relative_to(REPO_ROOT)
        branch_name = current_branch(REPO_ROOT)
        protected = get_protected_branches()
        if branch_name in protected:
            sys.exit(
                f"refusing to commit task-note bookkeeping while on {branch_name!r} in "
                f"{REPO_ROOT} \u2014 this must go through a {cfg['term_mr']} like every "
                "other change. `git checkout` the task's feature branch first."
            )
        r = run_git(REPO_ROOT, "add", str(rel))
        if r.returncode != 0:
            sys.exit(f"git add failed: {r.stderr}")
        r = run_git(
            REPO_ROOT, "commit", "-m",
            f"Mark {path.stem} done, link {cfg['repo_key']} {cfg['term_prefix']}{args.iid}"
        )
        if r.returncode != 0:
            sys.exit(f"git commit failed: {r.stderr}")
        if not args.no_push:
            nb_cfg = get_repo_cfg(repo_key)
            push_branch(nb_cfg, branch_name, args.retries, args.backoff)

    print(f"updated {path}")


def _detect_backend(repo_key: str) -> str:
    """Lightweight backend detection without full cfg (for list/validate)."""
    up = str(repo_key).upper()
    if os.environ.get(f"GITHUB_{up}_TOKEN"):
        return "github"
    if os.environ.get(f"GITLAB_{up}_TOKEN"):
        return "gitlab"
    return "???"


def cmd_validate(args):
    """Validate all task notes for correctness and consistency."""
    if not TASKS_DIR.is_dir():
        sys.exit(f"tasks directory not found: {TASKS_DIR}")

    errors = []
    warnings = []
    slugs = []

    for task_file in sorted(TASKS_DIR.glob("*.md")):
        if task_file.name.startswith("."):
            continue

        slug = task_file.stem

        # Check for duplicate slugs
        if slug in slugs:
            errors.append(f"{slug}: duplicate slug found")
        slugs.append(slug)

        # Parse frontmatter
        try:
            frontmatter, sections, title, _ = parse_task(task_file)
        except SystemExit as e:
            errors.append(f"{slug}: {e.code}")
            continue
        except Exception as e:
            errors.append(f"{slug}: failed to parse: {e}")
            continue

        # Check repo key
        repo_key = frontmatter.get("repo")
        if not repo_key:
            errors.append(f"{slug}: missing 'repo:' in frontmatter")
            continue

        repo_key = str(repo_key)

        # Check if token exists (warning, not error — file may exist before config is set)
        up = repo_key.upper()
        if not os.environ.get(f"GITHUB_{up}_TOKEN") and not os.environ.get(f"GITLAB_{up}_TOKEN"):
            warnings.append(f"{slug}: no matching token for repo '{repo_key}' (set GITHUB_{up}_TOKEN or GITLAB_{up}_TOKEN)")

        # Check required sections exist
        for section in ("Goal", "Definition of done", "Files / scope"):
            if section not in sections:
                warnings.append(f"{slug}: missing '## {section}' section")

    # Report
    if errors:
        for e in errors:
            print(f"ERROR: {e}", file=sys.stderr)
        print(f"\nValidation failed with {len(errors)} error(s).", file=sys.stderr)
        sys.exit(1)

    if warnings:
        for w in warnings:
            print(f"WARNING: {w}", file=sys.stderr)

    if not errors and not warnings:
        print(f"All {len(slugs)} task note(s) validated successfully.")
    else:
        print(f"Validation passed ({len(warnings)} warning(s), {len(errors)} error(s)).")


def cmd_list(args):
    """List all task notes with status, backend, branch, and blockers."""
    if not TASKS_DIR.is_dir():
        sys.exit(f"tasks directory not found: {TASKS_DIR}")

    rows = []
    for task_file in sorted(TASKS_DIR.glob("*.md")):
        if task_file.name.startswith("."):
            continue

        slug = task_file.stem

        try:
            frontmatter, sections, title, _ = parse_task(task_file)
        except SystemExit:
            rows.append({"slug": slug, "status": "???", "repo": "???", "backend": "???", "branch": "???", "blocked_by": 0})
            continue

        repo_key = frontmatter.get("repo", "???")
        backend = _detect_backend(repo_key)
        blocker_count = len(unresolved_blockers(sections))

        # Determine branch
        branch = "???"
        prefix = os.environ.get("AGENT_BRANCH_PREFIX")
        if prefix:
            handoff = sections.get("Handoff", "")
            m = re.search(r"branch `([^`]+)`", handoff)
            if m:
                branch = m.group(1)
            else:
                branch = f"{prefix}/{slug}"

        rows.append({
            "slug": slug,
            "status": frontmatter.get("status", "???"),
            "repo": repo_key,
            "backend": backend,
            "branch": branch,
            "blocked_by": blocker_count,
        })

    if not rows:
        print("No task notes found.")
        return

    # Print JSON output if --json flag is set, otherwise print table
    if args.json:
        print(json.dumps(rows, indent=2))
        return

    # Print table
    slug_w = max(len(r["slug"]) for r in rows)
    slug_w = max(slug_w, 4)  # minimum width for "Slug"
    status_w = max(len(r["status"]) for r in rows)
    status_w = max(status_w, 6)  # minimum for "Status"

    header = f"{'Slug':<{slug_w}}  {'Status':<{status_w}}  {'Backend':<8}  {'Repo':<15}  Branch  Blocked"
    sep = "-" * len(header)
    print(header)
    print(sep)
    for r in rows:
        blocked = f"+{r['blocked_by']}" if r['blocked_by'] > 0 else "no"
        print(f"{r['slug']:<{slug_w}}  {r['status']:<{status_w}}  {r['backend']:<8}  {r['repo']:<15}  {r['branch']}  {blocked}")


# ── Argument parsing ──────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("task",
                       help="task note path, filename, or slug under porting/TaskNotes/Tasks/")
        p.add_argument("--repo", help="override repo detection (uses frontmatter 'repo:' by default)")
        p.add_argument("--retries", type=int, default=6)
        p.add_argument("--backoff", type=int, default=15)
        p.add_argument("--no-glab", action="store_true", help="skip glab CLI")
        p.add_argument("--no-gh", action="store_true", help="skip gh CLI")

    # resolve
    p = sub.add_parser("resolve", help="print which repo/branch/backend a task maps to")
    common(p)
    p.add_argument("--branch", help="override branch detection")
    p.set_defaults(func=cmd_resolve)

    # push
    p = sub.add_parser("push", help="push the task's branch over HTTPS with retry/backoff")
    common(p)
    p.add_argument("--branch", help="override branch detection")
    p.add_argument("--force", action="store_true", help="force-push")
    p.set_defaults(func=cmd_push)

    # open-mr
    p = sub.add_parser("open-mr", help="open the MR/PR for the task's branch")
    common(p)
    p.add_argument("--branch", help="override branch detection")
    p.add_argument("--target", help="override target branch")
    p.add_argument("--title")
    p.add_argument("--description")
    p.add_argument("--force", action="store_true",
                   help="open even with unresolved Blocked by items")
    p.set_defaults(func=cmd_open_mr)

    # close-mr
    p = sub.add_parser("close-mr", help="close an MR/PR by number (cleanup / testing)")
    p.add_argument("repo", help="repo key (must match a *_<NAME>_TOKEN env var)")
    p.add_argument("iid", type=int)
    p.add_argument("--retries", type=int, default=6)
    p.add_argument("--backoff", type=int, default=15)
    p.add_argument("--no-glab", action="store_true", help="skip glab CLI")
    p.add_argument("--no-gh", action="store_true", help="skip gh CLI")
    p.set_defaults(func=cmd_close_mr)

    # delete-branch
    p = sub.add_parser("delete-branch", help="delete a remote branch (cleanup / testing)")
    p.add_argument("repo", help="repo key (must match a *_<NAME>_TOKEN env var)")
    p.add_argument("branch")
    p.add_argument("--retries", type=int, default=6)
    p.add_argument("--backoff", type=int, default=15)
    p.add_argument("--no-glab", action="store_true", help="skip glab CLI")
    p.add_argument("--no-gh", action="store_true", help="skip gh CLI")
    p.set_defaults(func=cmd_delete_branch)

    # finish
    p = sub.add_parser("finish", help="write status/Handoff MR/PR line into the task note and commit")
    common(p)
    p.add_argument("--iid", required=True, type=int)
    p.add_argument("--url", required=True)
    p.add_argument("--label", required=True, help='e.g. "myproject"')
    p.add_argument("--no-commit", action="store_true")
    p.add_argument("--no-push", action="store_true")
    p.set_defaults(func=cmd_finish)

    # validate
    p = sub.add_parser("validate", help="validate all task notes for correctness")
    p.set_defaults(func=cmd_validate)

    # list
    p = sub.add_parser("list", help="list all task notes with status and backend")
    p.add_argument("--json", action="store_true", help="output as JSON")
    p.set_defaults(func=cmd_list)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()