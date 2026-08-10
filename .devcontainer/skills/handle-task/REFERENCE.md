# handle-task reference

Lookup material for `SKILL.md`. Nothing here is needed to run the happy path — read it when the driver reports a missing variable, or when you need a subcommand the standard flow doesn't use.

## Env vars the driver reads

All are pre-exported by the devcontainer; `.devcontainer/open-code/.env.example` documents the names. If one is missing, stop and tell the user — don't supply it yourself.

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

`<NAME>` is an arbitrary identifier (e.g. `MYPROJECT`). The driver uses whichever token is set to determine the backend for that repo. Both `*_<NAME>_TOKEN` and `*_<NAME>_PATH` must match the same naming key. No hardcoded defaults — repository paths and tokens come only from these vars.

## `driver.py` subcommands

- `resolve`, `push`, `open-mr`, `finish` — normal task workflow
- `validate` — checks all task notes for valid frontmatter, required sections, and matching tokens
- `list` — lists all task notes with status, backend, branch, and blockers (add `--json` for machine-readable output)
- `close-mr <repo> <number>`, `delete-branch <repo> <branch>` — cleanup/testing

`askpass.sh` is the GIT_ASKPASS helper `push`/`finish` shell out to.

**CLI tool selection:** the driver uses whichever CLI matches the backend for each repo — `glab` for GitLab, `gh` for GitHub. Both are pre-installed. Use `--no-glab` or `--no-gh` on any command to skip that CLI (errors if the backend requires it).

**Retries and timeouts:** every subcommand that hits the network retries with backoff (`--retries 6 --backoff 15` by default) and has a hard per-attempt subprocess timeout (45s for git, 60-120s for the CLI tool) — `git push` can hang indefinitely with no error otherwise.
