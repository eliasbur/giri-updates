---
title: Sync the legacy jcode skill seed with the current skills
status: open             # open | done (driver rewrites this on `finish`)
priority: medium          # low | medium | high
repo: giriupdates          # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []              # e.g. dev, cockpit, gpu1
projects: []              # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0           # minutes
dateCreated: 2026-08-22
# dateModified / completedDate are added automatically by `driver.py finish`
---
## Goal
The jcode devcontainer's skill seed (`.devcontainer/jcode/skills/`) contains a copy of `handle-task` that is older than the current working skill (`.devcontainer/skills/handle-task/`), and a fresh container rebuilt from this seed ships a broken one: the seeded driver computes `REPO_ROOT = Path(__file__).resolve().parents[3]`, so at the seeded location it resolves to `/root` and `TASKS_DIR` becomes `/root/docs/TaskNotes/Tasks` (absent), while its SKILL.md text points agents at `docs/TaskNotes/Tasks/` which no longer exists in this repo. Done = the seed copy is byte-identical to the current working skill, and the same is true for the other skills that exist in only one of the two trees (`create-task` is missing from the seed).

## Approach
1. Compare the trees to establish the full delta, not just handle-task:
   `diff -r .devcontainer/jcode/skills .devcontainer/skills` (expect: stale handle-task, missing create-task, plus anything else).
2. Replace the seed with the current tree: `rm -rf .devcontainer/jcode/skills && cp -r .devcontainer/skills .devcontainer/jcode/skills` (do NOT keep the old tree around — the container's `postCreateCommand` in `.devcontainer/jcode/devcontainer.json` already syncs `.devcontainer/skills/*` into `/root/.jcode/skills/`, and that copy is the authoritative one).
3. Verify the seeded handle-task driver's `REPO_ROOT` no longer matters: `python3 .devcontainer/jcode/skills/handle-task/driver.py --help` must list the `validate` subcommand (the current driver is repo-robust: it walks up from CWD / its own path and finds `porting/TaskNotes/Tasks/` relative to the git root).
4. Grep the repo for dangling references to the old path: `grep -rn "docs/TaskNotes" .devcontainer AGENTS.md` — every hit either becomes `porting/TaskNotes` or the referencing file is deleted.
5. `driver.py check sync-legacy-skill-seed` (from the create-task skill) still passes after the change.

## Definition of done
- [ ] `diff -r .devcontainer/jcode/skills .devcontainer/skills` exits 0 (trees identical)
- [ ] Seeded handle-task driver supports `validate` and resolves a note under `porting/TaskNotes/Tasks/` (run `driver.py resolve porting/TaskNotes/Tasks/llvm-5-port.md` with CWD = repo root and confirm it names that path)
- [ ] No `docs/TaskNotes` references remain under `.devcontainer/jcode/` or `AGENTS.md` (`create-task/driver.py` keeps `docs/TaskNotes/Tasks` as a deliberate fallback candidate and must not be changed by this task)
- [ ] MR/PR opened into `development` and linked below

## Files / scope
.devcontainer/jcode/skills/            (replaced wholesale)
.devcontainer/jcode/devcontainer.json     (postCreateCommand — only if the diff in step 1 shows it is stale)
AGENTS.md                                 (only if it mentions docs/TaskNotes)
- Do **not** change `.devcontainer/skills/` (it is the source of truth), `/root/.jcode/` (container runtime state, re-seeded at container start), or any code under `lib/`, `include/`, or `runtime/`.

## Notes
- Discovered while building the create-task skill (session 2026-08-22): the seeded handle-task is a docs/-lineage copy whose driver computes REPO_ROOT=parents[3] of its own file, so in the seeded location it resolves to /root and points at /root/docs/TaskNotes/Tasks (absent).

## Blocked by
- ~~none~~

## Progress log

## Handoff
- branch `agent/open-code/sync-legacy-skill-seed`
Refs: AGENTS.md, .devcontainer/jcode/devcontainer.json, .devcontainer/jcode/Dockerfile, porting/TaskNotes/Tasks/.task-template.md
