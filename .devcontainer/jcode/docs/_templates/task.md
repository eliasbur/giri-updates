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
status: open
priority: medium        # low | medium | high
contexts: []            # e.g. dev, cockpit, gpu1
projects: []            # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0         # minutes
dateCreated: YYYY-MM-DD
---
```

Body sections:

- `## Goal` — one sentence: the outcome.
- `## Definition of done` — checklist; last item is always "MR opened into `development` and linked below".
- `## Files / scope` — paths to touch.
- `## Handoff` — worker (<AGENT_NAME>), branch `agent/<AGENT_NAME>/<slug>`, MR URL, `Refs:` to docs.
