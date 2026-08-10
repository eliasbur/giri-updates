---
name: handle-task
description: Pick up a task note from porting/TaskNotes/Tasks/, implement the required sub-tasks yourself, committing and pushing after each one with a matching entry in the note's Progress log, then hand it off via a merge request or pull request. Use when asked to handle, work, start, resume, or pick up a task, or to open/finish an MR/PR for one.
---

Task notes in `porting/TaskNotes/Tasks/*.md` (Obsidian TaskNotes format) describe scoped units of work: a Goal, a Definition of done checklist, Files/scope, optional Blocked-by, a `## Progress log` the working agent appends to as it commits, and a Handoff section. Handling one means: implement the DoD items — committing and pushing after each one, with a matching entry in the note's `## Progress log` — then open an MR (GitLab) or PR (GitHub) over **HTTPS with a token**, never SSH.

**The branch is the handoff.** Agents lose their context window, crash, or get killed mid-task routinely. Anything that lives only in your context or only in an uncommitted working tree is lost when that happens, and the next agent restarts from zero. Commit and push after every sub-task, and write down in the task note what each commit did and what you were about to do next — that is what makes an interrupted task resumable instead of restartable.

The backend (GitLab vs GitHub) per repo is determined by which env var is set: `GITHUB_<NAME>_TOKEN` → GitHub (`github.com` / `gh`), or `GITLAB_<NAME>_TOKEN` → GitLab (`git.tu-berlin.de` / `glab`). Each repo can use a different backend independently.

All credential-sensitive mechanics — picking the repo, pushing over HTTPS, opening/closing MRs/PRs via the CLI, and writing the Handoff/status fields back into the task note — go through `.claude/skills/handle-task/driver.py`. Primary operations use the appropriate CLI (`glab` or `gh`) with automatic retry/backoff. Don't hand-roll `git push` with a token in the URL: it leaks the token into `ps` output on this shared host. `REFERENCE.md` in this directory documents the driver's full subcommand list and every env var it reads.

## Setup

**None.** The devcontainer sourced `.devcontainer/open-code/.env` (gitignored) at build time, so every variable the driver reads is already exported in your shell (`REFERENCE.md` lists them). Do **not** run `source .env` — there is no `.env` at the repo root, and the driver never reads the file itself; it only reads the environment. The tooling it needs (`git`, `python3`, `pyyaml`, `glab`, `gh`) is pre-installed in the image.

If the driver exits complaining that a variable is missing (`no token for repo '<key>'`, `missing GITHUB_<KEY>_PATH`, `missing AGENT_BRANCH_PREFIX in environment`), **stop and tell the user** which variable the driver asked for. Do not try to supply it yourself — not by sourcing a file, not by exporting it inline, not by guessing a value.

**Required task frontmatter** — each task note must include `repo: <name>` in its YAML frontmatter, whose value matches a `<NAME>` suffix in the env vars (case-insensitive). Missing `repo:` causes the driver to exit with an error. See `porting/TaskNotes/Tasks/.task-template.md` for the full template.

## Run

1. **Resolve** the task — repo, backend, branch, blockers:
   ```bash
   /usr/bin/python3 .claude/skills/handle-task/driver.py resolve porting/TaskNotes/Tasks/<slug>.md
   ```
   Prints JSON including `repo`, `backend` (`github`/`gitlab`), `term_mr` (`PR`/`MR`), `term_prefix` (`#`/`!`), `repo_root`, `branch`, `target_branch` (from `TARGET_BRANCH` env or `development`), and `blocked_by` (non-empty ⇒ stop and surface it — don't `--force` past a real blocker).

2. **Resume check** — before touching anything, find out whether a previous agent already worked this task and was interrupted:
   ```bash
   cd <repo_root> && git fetch && git log --oneline <target_branch>..<branch>   # <branch> from resolve
   ```
   If that branch exists and has commits, **do not start over**: read the `## Progress log` section of the task note, `git checkout <branch>`, skip step 5 (branch creation), rebuild the TODO list in step 4 from the remaining unchecked `Definition of done` items, and continue from the "next:" pointer of the last progress-log entry.

   Also check for a stash the interrupted run never restored — `git stash list | grep "stash/agent-<slug>"`. If one is there, leave it stashed for now and restore it in step 12 as usual; just don't create a second one in step 3.

   Resuming that branch is *not* a violation of the "never push pre-existing code as your work" rule in step 6. That rule is about unattributed changes you find lying in the working tree; commits on the task's own branch that are recorded in the progress log are this task's own history. Anything on the branch that is *not* accounted for by a progress-log entry falls under the step 6 rule — reimplement it yourself rather than adopting it.

3. **Clean workspace** — checkout the target branch in the target `repo_root` first (skip if you are resuming a branch from step 2). If there are uncommitted changes in tracked files relevant to this task's scope, stash them under a task-scoped name; step 12 restores them.
   ```bash
   cd <repo_root> && git checkout <target_branch>
   git stash push -m "stash/agent-<slug>" -- <file-or-path1> <file-or-path2>   # only files in Files/scope
   ```
   ⚠️ It must be `git stash push -m <msg> -- <paths>`. **`git stash save` does not accept pathspecs** — it reads every argument as part of the message, so `git stash save "stash/agent-<slug>" file1 file2` quietly stashes the *entire* working tree, which on this shared checkout means swallowing other people's in-progress work. Never stash untracked files and never run a bare `git stash` (see Gotchas); if a file in your scope is untracked, leave it where it is and work around it.

4. **Plan** — use the built-in **plan agent** to break the work into an actionable TODO list. Switch to the `plan` agent (read-only, cannot modify files, cannot run bash) and ask it to expand every `Definition of done` item from the task note into concrete sub-tasks. Include implementation details, edge cases, test scenarios, and any files that need updating beyond what the task explicitly lists. For each sub-task, capture a one-line description and the files it touches.

   After planning, use `todowrite` to create a structured TODO list of all sub-tasks (status: `pending`). This is your execution plan — commit it to memory.

5. **Create the branch** yourself in `repo_root` from the target branch (ordinary `git checkout -b`, not driver-mediated — this part has no credentials involved). Use the `branch` value from `resolve` verbatim — it is already the complete branch name, `AGENT_BRANCH_PREFIX` included, and `push`/`open-mr` will look for exactly that name:
   ```bash
   cd <repo_root> && git checkout -b <branch> <target_branch>   # <branch> exactly as resolve printed it
   ```

6. **Implement** — work through the TODO list sub-tasks one by one. After each completed sub-task, mark it `completed` in `todowrite` and immediately run the commit loop in step 7 — do not batch several sub-tasks into one commit at the end. Keep refining the list throughout:
   - If you discover new work that wasn't planned (missing imports, related tests, type changes), add new TODO items immediately.
   - If a sub-task turns out to be more complex than expected, break it into smaller pieces and split the TODO.
   - Check `todowrite` before and after every implementation session to stay on track.

     **Never push pre-existing code as your work.** If the task's Files/scope has already been modified in the branch (e.g. by a cockpit session or another agent), you must implement the items yourself — writing code from the requirements, not from the pre-existing diff. Treat cockpit reference patches as guidance only. If you cannot or will not implement the code yourself, do **not** open an MR/PR claiming the task is done.

     **Test your changes whenever possible.** Run existing tests, execute the relevant CLI commands, or manually verify the behaviour — whatever is feasible given the scope of the change. Mention the test results in the MR/PR description so reviewers know the changes were validated.

     Only `git add` the files you touched — never `git add -A`/`git add .`. This is a shared checkout; other tasks are routinely being worked concurrently in the same working tree, and a broad add will scoop up someone else's WIP.

7. **Commit, document, push — after every sub-task.** This is not optional bookkeeping; it is what lets another agent pick the task up when you run out of context or die mid-run. Run all three parts each time you mark a TODO `completed`:

   a. **Document the change in the task note** under `## Progress log` — create that section just above `## Handoff` if the note doesn't have one yet. One line per commit, newest last, and always including what comes next:
      ```markdown
      ## Progress log
      - 2026-08-10 `a1b2c3d` — Replaced `getOrInsertFunction` calls with the LLVM 5 arg-pack overload in Instrumentor.cpp. TODO 3/7 done; next: update the matching call sites in Tracing.cpp.
      ```
      The `next:` clause is the part that matters most to a successor — it is the only record of what you *intended* to do, which no diff can reconstruct. Also note anything you learned that isn't visible in the diff: a dead end you ruled out, an API that behaves differently than the release notes claim, a test that is expected to fail until a later sub-task lands.

   b. **Commit the code and the note together**, with explicit paths — never `git add -A`/`git add .`:
      ```bash
      cd <repo_root> && git add <file1> <file2> porting/TaskNotes/Tasks/<slug>.md
      git commit -m "<imperative summary of the sub-task>"
      git rev-parse --short HEAD    # fill this sha into the progress-log line
      ```
      Either write the log line first and fill the sha in with the *next* commit, or commit and then `git commit --amend` the note — just don't leave a placeholder sha behind. If `*_<NAME>_LOCAL_PATH` is not `.`, the code and the note live in two different checkouts and need two commits; see Gotchas.

   c. **Push the branch** so the work outlives this checkout and this session:
      ```bash
      /usr/bin/python3 .claude/skills/handle-task/driver.py push porting/TaskNotes/Tasks/<slug>.md
      ```
      Same command as step 9, safe to run as often as you like: it refuses protected branches and retries the flaky GitLab remote for you. An unpushed commit is one `docker rm` away from being gone — push at least once per sub-task, and more often if a sub-task runs long.

   A half-finished sub-task at the end of your context is still worth committing: commit it, describe in the progress log exactly how far it got and what is broken, and push.

8. **Verify completeness** — before opening the MR/PR, check that every TODO is marked `completed`. Review each against what you actually wrote (not against what happened to be in the branch). If any TODO is still `pending`, implement it first. Check that the progress log has an entry for every commit on the branch, and that `git status` shows nothing uncommitted in your Files/scope.

9. **Final push** — the step 7 loop has usually pushed everything already; run it once more so the remote branch definitely matches your tree (retries transient flakiness automatically — see Gotchas):
   ```bash
   /usr/bin/python3 .claude/skills/handle-task/driver.py push porting/TaskNotes/Tasks/<slug>.md
   ```

10. **Open the MR/PR** (uses the `term_mr` from `resolve` output — "MR" for GitLab, "PR" for GitHub):
    ```bash
    /usr/bin/python3 .claude/skills/handle-task/driver.py open-mr porting/TaskNotes/Tasks/<slug>.md \
      --title "<imperative summary>" --description "Implements porting/TaskNotes/Tasks/<slug>.md."
    ```
    Prints `{"repo": ..., "type": "MR|PR", "number": ..., "web_url": ...}`. Refuses to run if `resolve` would report unresolved blockers (override with `--force` only if you've confirmed the blocker is actually resolved but not yet struck through in the note).

11. **Finish** — write status/Handoff back into the task note, then commit+push it *onto the task's own feature branch* (the one with the open MR/PR from step 10), so it rides into the target branch through that same MR/PR — never a direct push to a protected branch:
    ```bash
    /usr/bin/python3 .claude/skills/handle-task/driver.py finish porting/TaskNotes/Tasks/<slug>.md \
      --iid <number> --url <web_url> --label <repo-name>
    ```
    This sets `status: done`, adds `completedDate`/updates `dateModified`, and fills in the `## Handoff` handoff line. The format depends on the backend:
    - **GitLab:** `- MR: <label> !<number> <url>`
    - **GitHub:** `- PR: <label> #<number> <url>`

    It commits and pushes that one file to whatever branch is currently checked out. It refuses (see Gotchas — "never bypass review") if that's the target branch or `main`: `git checkout` the task's feature branch first.

    `finish` does **not** touch the `Definition of done` checkboxes — check those off yourself with Edit before running it, since only you know which items actually landed. It stages *only* the task note file, so it's safe to run even with unrelated concurrent WIP sitting in the tree — including the progress-log edits from step 7, which it leaves untouched.

    Leave the `## Progress log` in the note; it is the record of how the work was done and it merges into the target branch with everything else. Clear the TODO list with `todowrite` after the MR/PR is open.

12. **Restore the stash** — if step 3 stashed anything, put it back before you end the session. Those were somebody's uncommitted changes, and a stash entry they never made is somewhere they will not think to look.
    ```bash
    cd <repo_root> && git checkout <target_branch>
    git stash list | grep "stash/agent-<slug>"   # find its stash@{n}
    git stash pop stash@{<n>}
    ```
    Pop it onto the target branch — that is where it was taken from, and it hasn't moved, since your work is still sitting in an unmerged MR/PR. Pop by explicit `stash@{n}`, never bare `git stash pop`, which takes whatever is on top of a stack you share with other sessions.

    If the pop conflicts, **do not force it and do not drop the stash**: `git checkout .` is forbidden here, so reset the attempt with `git checkout -- <the conflicted paths>`, leave the entry in the stash list, and flag it — name the stash and the conflicting files in the progress log and in a comment on the MR/PR, so a human can resolve it against the merged result.

## Gotchas

- **`git.tu-berlin.de` (GitLab) is flaky.** Verified live: `git push` over HTTPS intermittently hangs forever on `POST git-receive-pack` (no error, no timeout — a bare `git push` can block the whole session), and separately returns `500 ... pre-receive hook declined` or `503 no available server` under load. None of these mean the push actually failed in a way that needs a different approach — they're transient. The driver retries automatically; if you're doing something outside the driver, always wrap `git push`/CLI calls in a timeout + retry loop yourself, never a bare call.
- **`glab` is installed natively** and works non-interactively with `GLAB_NO_PROMPT=true`. The driver uses it for GitLab MR operations.
- **`gh` is installed natively** and works non-interactively with `GH_TOKEN`. The driver uses it for GitHub PR operations.
- **Never put the token in a URL or CLI arg** (`https://oauth2:$TOKEN@...`, `curl -H "PRIVATE-TOKEN: $TOKEN"`) — this is a shared multi-user host and that's visible via `ps aux` for the life of the process. Use the driver, which keeps tokens in env vars read by an askpass script.
- **This working tree is live-shared.** While verifying this skill, files have appeared with uncommitted changes mid-session — someone else's task in progress. Never `git add -A` or `git checkout .` in this repo. Bare `git stash`, `-u`/`--include-untracked`, `-a`/`--all` and bare `git stash pop` are also forbidden — untracked files here are other people's new files and build artifacts, so stashing them makes them vanish from under a session still using them, and a bare pop takes an entry that isn't yours. **Exception:** a targeted `git stash push -m "..." -- <specific-files>` of tracked files in your task's scope, popped by explicit `stash@{n}` (steps 3 and 12). Always use `git add` with explicit paths.
- **`Blocked by` items are struck through (`~~text~~`) when resolved**, not removed — that's how `resolve`/`open-mr` tell a resolved blocker from an active one. If you resolve a blocker, edit the note to wrap it in `~~ ~~` rather than deleting the line.
- **Never bypass review — not even for task-note bookkeeping.** Every change, including task-note status updates, must go through an MR (GitLab) or PR (GitHub). The driver hard-refuses any push whose target is the protected branch (`push_branch()` checks `PROTECTED_BRANCHES`), and `cmd_finish` commits to whatever branch is currently checked out — so this is enforced by the driver, not just by convention in this doc.
- **Never start a progress-log line with `- MR:` or `- PR:`.** `cmd_finish` replaces the *first* line anywhere in the note body matching `^- (MR|PR):` with the real handoff link — it does not restrict itself to the `## Handoff` section. A progress-log entry in that shape gets silently overwritten and the actual handoff line lands in the wrong place. Start entries with the date, as shown in step 7.
- **Split checkouts need two commits.** When `*_<NAME>_LOCAL_PATH` is not `.`, the code (`repo_root` from `resolve`) and the task note (this repo) are separate git checkouts, so step 7b is two `git add`/`git commit` pairs, and `driver.py push` only pushes the code repo — it runs `git push` in `local_root`. The note repo has to be pushed by hand, and the same mismatch affects `finish`, which commits the note in this repo but then invokes the push against `local_root`. With the default `LOCAL_PATH=.` none of this applies.