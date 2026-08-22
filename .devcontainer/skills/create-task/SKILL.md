---
name: create-task
description: Persist the current goal as a durable task note so a later session can pick the work up with the handle-task skill. Use when asked to create, persist, schedule, or queue a task; when a plan-mode discussion has just settled on a goal; or to self-schedule future work. Scaffolds the note, proves handle-task can resolve it, and commits it.
---

Creating a task means turning "what we just agreed to do" into a task note under `porting/TaskNotes/Tasks/` that a **later, context-free session** can resolve, resume, and hand off via the `handle-task` skill. The note is the handoff: everything that must survive (goal, DoD, scope, branch, blockers) goes into the file, nothing lives only in this session's context.

**Write it for a stranger who never saw the discussion.** The next agent reads only the note. If a fact from the discussion isn't in the note, it doesn't exist.

## Driver

Mechanical work goes through the driver next to this file (it lives beside the `handle-task` skill and locates that driver itself for the round-trip check):

```bash
DRIVER=$(dirname "$(readlink -f .claude/skills/handle-task/SKILL.md)")/../create-task/driver.py
python3 $DRIVER new <slug> --repo <key> --title "Imperative summary" [--note "seed line"] [--blocked-by <slug>]
python3 $DRIVER check <slug>
python3 $DRIVER commit <slug>
```

The driver prints JSON. It needs `AGENT_BRANCH_PREFIX` in the environment (for the Handoff branch line); it never needs tokens.

## Workflow

1. **Agree on the shape first.** Before scaffolding, settle: one-line Goal, verifiable Definition-of-done items (each independently checkable, last one always the MR/PR), files/scope with explicit exclusions, which `repo` key, and any `--blocked-by` slugs. If the goal can't be reduced to that shape, the plan isn't done yet — finish the discussion.

2. **`new <slug>`** — kebab-case slug, ≤ 49 chars. The scaffold fills frontmatter and all sections per the repo contract (`porting/TaskNotes/Tasks/.task-template.md`), seeds `branch $AGENT_BRANCH_PREFIX/<slug>` in `## Handoff`, and marks unfinished parts with `TODO:` so nothing is silently empty.

3. **Fill every `TODO:`** — this is the part the driver can't do. Write from the discussion:
   - `## Goal`: one sentence, the outcome, the state that counts as done.
   - `## Approach`: ordered phases with concrete commands/paths; this guides `handle-task`'s planning step, so keep every step executable.
   - `## Definition of done`: `- [ ]` checkboxes, concrete and checkable; keep the scaffold's final MR/PR item (it already names the target branch).
   - `## Files / scope`: explicit paths + "Do **not** change ..." exclusions.
   - `## Notes`: hazards, reference docs, environment quirks the working agent would otherwise have to rediscover.
   - `## Blocked by`: slugs of tasks that must finish first. With none, leave the `- ~~none~~` line. Unstruck entries make `handle-task` refuse `open-mr` — intentional.
   - Leave `## Progress log` empty (the working agent writes it, one line per commit).
   - `## Handoff`: keep the backticked `branch ...` line (the driver requires it); add `Refs:` to the docs that back the task.

4. **`check <slug>`** — hard gate. It validates the structure *and* runs the `handle-task` driver's own `resolve` on the note, so a later session is guaranteed to find the right repo, backend, and branch. It fails on unfilled `TODO:` lines, missing sections, or an empty DoD. Only proceed when it exits 0. (A `NOTE:` warning about a missing repo token is expected if the token isn't in this session — the note is still valid; `handle-task` asks for it at pickup.)

5. **`commit <slug>`** — commits exactly that one file (`git add <path>`, never `-A`; this is a shared checkout). Leave it on the current branch; `handle-task` commits the note *again* onto the task's feature branch at handoff, so this commit is just insurance against a lost checkout. Do **not** push or open an MR here — an open MR with an empty branch would collide with the one `handle-task` opens; that's all `handle-task`'s job.

6. **Report** the note path, slug, repo/branch, and target branch back to the user, and tell them how to resume: in any later session, ask to `handle-task <slug>`.

## Rules

- **One note per task.** Slugs are unique; the driver refuses to overwrite. Related work = separate notes, linked via `## Blocked by` or `## Notes`.
- **The DoD is the contract.** Vague items ("make it work") fail at `handle-task`'s completeness gate. Every item needs an observable, checkable end state.
- **No credential handling.** Tokens, pushing, MRs, and finishing belong to `handle-task`. This skill creates and verifies the note only; `check`/`commit` are read-only with respect to remotes.
- **Don't pre-check DoD boxes.** They start unchecked; ticking them is the working agent's job as it implements.
