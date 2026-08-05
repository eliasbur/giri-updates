---
name: handle-task
description: Pick up a task note from porting/TaskNotes/Tasks/, implement the required sub-tasks yourself, then hand it off via a merge request or pull request. Do NOT just tick checkboxes on unimplemented work — actually write the code for each Definition of Done item before opening the MR/PR. Use when asked to handle, work, start, or pick up a task, or to open/finish an MR/PR for one.
---

Task notes in `porting/TaskNotes/Tasks/*.md` (Obsidian TaskNotes format) describe scoped units of work: a Goal, a Definition of done checklist, Files/scope, optional Blocked-by, and a Handoff section. Handling one means: implement the DoD items, then push and open an MR (GitLab) or PR (GitHub) — over **HTTPS with a token**, never SSH.

The backend (GitLab vs GitHub) per repo is determined by which env var is set: `GITHUB_<NAME>_TOKEN` → GitHub (`github.com` / `gh`), or `GITLAB_<NAME>_TOKEN` → GitLab (`git.tu-berlin.de` / `glab`). Each repo can use a different backend independently.

All credential-sensitive mechanics — picking the repo, pushing over HTTPS, opening/closing MRs/PRs via the CLI, and writing the Handoff/status fields back into the task note — go through `.claude/skills/handle-task/driver.py`. Primary operations use the appropriate CLI (`glab` or `gh`) with automatic retry/backoff. Don't hand-roll `git push` with a token in the URL: it leaks the token into `ps` output on this shared host.

**Environment variables:** The driver reads tokens from environment variables. These are automatically available because your devcontainer started with env vars sourced from `.env` (gitignored). You do **not** need to `source .env` manually, and the driver does **not** read `.env` directly. See `.devcontainer/open-code/.env.example` for expected variable names.

## Setup

```bash
set -a; source .env; set +a   # loads tokens and paths
```

No other setup — the driver only needs `git`, `python3`, `pyyaml`, and `glab`/`gh` (all pre-installed).

**Required env vars** (read from `.env`):

| Env var | Description |
|---|---|
| `GITHUB_<NAME>_TOKEN` | PAT for a repo on github.com |
| `GITHUB_<NAME>_PATH` | Repository path (`owner/repo`) |
| `GITHUB_<NAME>_LOCAL_PATH` | Local filesystem path relative to repo root (default: `.`) |
| `GITLAB_<NAME>_TOKEN` | Project PAT for a repo on git.tu-berlin.de |
| `GITLAB_<NAME>_PATH` | GitLab project path (e.g. `group/subgroup/project`) |
| `GITLAB_<NAME>_LOCAL_PATH` | Local filesystem path relative to repo root (default: `.`) |
| `TARGET_BRANCH` | Default target branch for MRs/PRs (default: `development`) |
| `AGENT_BRANCH_PREFIX` | Prefix for agent-created branches (required, no default) |
| `ASSIGNEE` | GitHub/GitLab username to assign as reviewer on created MRs/PRs (optional) |

`<NAME>` is an arbitrary identifier (e.g. `MYPROJECT`). The driver uses whichever token is set to determine the backend for that repo. Both `*_<NAME>_TOKEN` and `*_<NAME>_PATH` must match the same naming key.

**Required task frontmatter** — each task note must include `repo: <name>` in its YAML frontmatter. The value must match a `<NAME>` suffix in the env vars (case-insensitive). Missing `repo:` causes the driver to exit with an error. See `porting/TaskNotes/Tasks/.task-template.md` for the full template.

Required task frontmatter — each task note must include:
```yaml
repo: myproject
```
The value must match a `<NAME>` suffix in the env vars (case-insensitive). Missing `repo:` causes the driver to exit with an error.

## Run (agent path)

1. **Resolve** the task — repo, backend, branch, blockers:
   ```bash
   /usr/bin/python3 .claude/skills/handle-task/driver.py resolve porting/TaskNotes/Tasks/<slug>.md
   ```
   Prints JSON including `repo`, `backend` (`github`/`gitlab`), `term_mr` (`PR`/`MR`), `term_prefix` (`#`/`!`), `repo_root`, `branch`, `target_branch` (from `TARGET_BRANCH` env or `development`), and `blocked_by` (non-empty ⇒ stop and surface it — don't `--force` past a real blocker).

2. **Clean workspace** — checkout the target branch in the target `repo_root` first. If there are uncommitted changes in files relevant to this task's scope, stash them in a task-scoped stash so you can restore them after the MR/PR merges.
   ```bash
   cd <repo_root> && git checkout <target_branch>
   git stash save "stash/agent-<slug>" <file-or-path1> <file-or-path2>  # only stash files in Files/scope
   ```
   ⚠️ **Never `git stash` untracked files** (use `--include-untracked` only if necessary). Also avoid `git stash --all` or bare `git stash` — this is a shared checkout and a broad stash will pull in other people's WIP and untracked build artifacts. Target only the specific files in scope.

3. **Plan** — use the built-in **plan agent** to break the work into an actionable TODO list. Switch to the `plan` agent (read-only, cannot modify files, cannot run bash) and ask it to expand every `Definition of done` item from the task note into concrete sub-tasks. Include implementation details, edge cases, test scenarios, and any files that need updating beyond what the task explicitly lists. For each sub-task, capture a one-line description and the files it touches.

   After planning, use `todowrite` to create a structured TODO list of all sub-tasks (status: `pending`). This is your execution plan — commit it to memory.

4. **Create the branch** yourself in `repo_root` from the target branch (ordinary `git checkout -b`, not driver-mediated — this part has no credentials involved):
   ```bash
   cd <repo_root> && git checkout -b ${AGENT_BRANCH_PREFIX}/<branch> <target_branch>
   ```

5. **Implement** — work through the TODO list sub-tasks one by one. After each completed sub-task, mark it `completed` in `todowrite`. Keep refining the list throughout:
   - If you discover new work that wasn't planned (missing imports, related tests, type changes), add new TODO items immediately.
   - If a sub-task turns out to be more complex than expected, break it into smaller pieces and split the TODO.
   - Check `todowrite` before and after every implementation session to stay on track.

     **Never push pre-existing code as your work.** If the task's Files/scope has already been modified in the branch (e.g. by a cockpit session or another agent), you must implement the items yourself — writing code from the requirements, not from the pre-existing diff. Treat cockpit reference patches as guidance only. If you cannot or will not implement the code yourself, do **not** open an MR/PR claiming the task is done.

     **Test your changes whenever possible.** Run existing tests, execute the relevant CLI commands, or manually verify the behaviour — whatever is feasible given the scope of the change. Mention the test results in the MR/PR description so reviewers know the changes were validated.

     Only `git add` the files you touched — never `git add -A`/`git add .`. This is a shared checkout; other tasks are routinely being worked concurrently in the same working tree, and a broad add will scoop up someone else's WIP.

6. **Verify completeness** — before pushing, check that every TODO is marked `completed`. Review each against what you actually wrote (not against what happened to be in the branch). If any TODO is still `pending`, implement it first.

7. **Push** (retries transient flakiness automatically — see Gotchas):
   ```bash
   /usr/bin/python3 .claude/skills/handle-task/driver.py push porting/TaskNotes/Tasks/<slug>.md
   ```

8. **Open the MR/PR** (uses the `term_mr` from `resolve` output — "MR" for GitLab, "PR" for GitHub):
   ```bash
   /usr/bin/python3 .claude/skills/handle-task/driver.py open-mr porting/TaskNotes/Tasks/<slug>.md \
     --title "<imperative summary>" --description "Implements porting/TaskNotes/Tasks/<slug>.md."
   ```
   Prints `{"repo": ..., "type": "MR|PR", "number": ..., "web_url": ...}`. Refuses to run if `resolve` would report unresolved blockers (override with `--force` only if you've confirmed the blocker is actually resolved but not yet struck through in the note).

9. **Finish** — write status/Handoff back into the task note, then commit+push it *onto the task's own feature branch* (the one with the open MR/PR from step 8), so it rides into the target branch through that same MR/PR — never a direct push to a protected branch:
   ```bash
   /usr/bin/python3 .claude/skills/handle-task/driver.py finish porting/TaskNotes/Tasks/<slug>.md \
     --iid <number> --url <web_url> --label <repo-name>
   ```
   This sets `status: done`, adds `completedDate`/updates `dateModified`, and fills in the `## Handoff` handoff line. The format depends on the backend:
   - **GitLab:** `- MR: <label> !<number> <url>`
   - **GitHub:** `- PR: <label> #<number> <url>`

   It commits and pushes that one file to whatever branch is currently checked out. It refuses (see Gotchas — "never bypass review") if that's the target branch or `main`: `git checkout` the task's feature branch first.

   `finish` does **not** touch the `Definition of done` checkboxes — check those off yourself with Edit before running it, since only you know which items actually landed. It stages *only* the task note file, so it's safe to run even with unrelated concurrent WIP sitting in the tree.

   Clear the TODO list with `todowrite` after the MR/PR is open.

## Run (human path)

Same driver, run by hand instead of by an agent — the commands above are already the direct path; there's no separate GUI/CLI wrapper.

## The driver

`driver.py` subcommands:
- `resolve`, `push`, `open-mr`, `finish` — normal task workflow
- `validate` — checks all task notes for valid frontmatter, required sections, and matching tokens
- `list` — lists all task notes with status, backend, branch, and blockers (add `--json` for machine-readable output)
- `close-mr <repo> <number>`, `delete-branch <repo> <branch>` — cleanup/testing

`askpass.sh` is the GIT_ASKPASS helper `push`/`finish` shell out to.

**CLI tool selection:** The driver uses whichever CLI tool matches the backend for each repo: `glab` for GitLab, `gh` for GitHub. Both are pre-installed. Use `--no-glab` or `--no-gh` on any command to skip that specific CLI tool (errors if the backend requires it).

**Repo config:** Repository paths and tokens are read from `*_<NAME>_TOKEN` and `*_<NAME>_PATH` env vars. No hardcoded defaults.

Every subcommand that hits the network retries with backoff (`--retries 6 --backoff 15` by default) and has a hard per-attempt subprocess timeout (45s for git, 60-120s for the CLI tool) — `git push` can hang indefinitely with no error otherwise.

## Gotchas

- **`git.tu-berlin.de` (GitLab) is flaky.** Verified live: `git push` over HTTPS intermittently hangs forever on `POST git-receive-pack` (no error, no timeout — a bare `git push` can block the whole session), and separately returns `500 ... pre-receive hook declined` or `503 no available server` under load. None of these mean the push actually failed in a way that needs a different approach — they're transient. The driver retries automatically; if you're doing something outside the driver, always wrap `git push`/CLI calls in a timeout + retry loop yourself, never a bare call.
- **`glab` is installed natively** and works non-interactively with `GLAB_NO_PROMPT=true`. The driver uses it for GitLab MR operations.
- **`gh` is installed natively** and works non-interactively with `GH_TOKEN`. The driver uses it for GitHub PR operations.
- **Never put the token in a URL or CLI arg** (`https://oauth2:$TOKEN@...`, `curl -H "PRIVATE-TOKEN: $TOKEN"`) — this is a shared multi-user host and that's visible via `ps aux` for the life of the process. Use the driver, which keeps tokens in env vars read by an askpass script.
- **This working tree is live-shared.** While verifying this skill, files have appeared with uncommitted changes mid-session — someone else's task in progress. Never `git add -A` or `git checkout .` in this repo. Bare `git stash` and `git stash --all` are also forbidden — they will pull in other people's WIP and untracked build artifacts. **Exception:** a targeted `git stash save "..." <specific-files>` of tracked files in your task's scope is allowed (see step 2). Always use `git add` with explicit paths.
- **`Blocked by` items are struck through (`~~text~~`) when resolved**, not removed — that's how `resolve`/`open-mr` tell a resolved blocker from an active one. If you resolve a blocker, edit the note to wrap it in `~~ ~~` rather than deleting the line.
- **Never bypass review — not even for task-note bookkeeping.** Every change, including task-note status updates, must go through an MR (GitLab) or PR (GitHub). The driver hard-refuses any push whose target is the protected branch (`push_branch()` checks `PROTECTED_BRANCHES`), and `cmd_finish` commits to whatever branch is currently checked out — so this is enforced by the driver, not just by convention in this doc.
- **`repo:` frontmatter is required.** Every task note must have `repo: <name>` in its YAML frontmatter. The driver fatal-errors if this is missing. Make sure the `<name>` matches (case-insensitively) a `*_<NAME>_TOKEN` environment variable.