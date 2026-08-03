---
tags:
  - template
---
# Task template

Copy the frontmatter below into a new note in `TaskNotes/Tasks/`. The `#task` tag is
what TaskNotes indexes; leave scheduling (`due` / `scheduled`) for the researcher.

```yaml
---
title: <imperative summary>
status: open             # open | done (driver rewrites this on `finish`)
priority: medium          # low | medium | high
repo: <repo-key>          # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []              # e.g. dev, cockpit, gpu1
projects: []              # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0           # minutes
dateCreated: YYYY-MM-DD
# dateModified / completedDate are added automatically by `driver.py finish`
---
```

Body sections:

- `## Goal` — one sentence: the outcome.
- `## Definition of done` — checklist; last item is always "MR opened into `development` and linked below".
- `## Files / scope` — paths to touch.
- `## Blocked by` — bullet list of blocking task slugs/refs; strike through (`~~...~~`) once resolved. An unstruck entry blocks `open-mr` unless `--force` is passed.
```markdown
  ## Blocked by
  - other-task-slug
  - ~~already-resolved-task~~
```
- `## Handoff` — worker (`<AGENT_NAME>`), branch, MR/PR line, `Refs:` to docs.
```markdown
  ## Handoff
  - branch `agent/<AGENT_NAME>/<slug>`
  - MR: <label> !123 https://git.example/...
  Refs: <docs>
```
  The branch line must literally contain `` branch `<name>` `` (backticked) or the driver falls back to `<AGENT_BRANCH_PREFIX>/<task-slug>`. The MR/PR line is auto-written/replaced by `driver.py finish` in the form `- {PR|MR}: <label> {#|!}<iid> <url>`.
