# porting/

Porting infrastructure — shared across all `port/llvm-*` branches. Kept separate from `docs/`
(which contains original project documentation: blog posts, GSoC reports, etc.).

| Path | Purpose |
|---|---|
| `AgentGuide.md` | Detailed build/test/debugging commands for agents |
| `HowItWorks.md` | Deep dive into Giri's tracing/slicing pipeline |
| `llvm-releases/` | Structured LLVM version change data (per version) |
| `TaskNotes/Tasks/` | Agentic task notes for the handle-task skill |

## Branch structure

```
master              ← clean fork base + all porting infrastructure
  ├─port/llvm-8.0.0   ← target branch for LLVM 8 port (agent PRs merge here)
  ├─port/llvm-14.0.0  ← target branch for LLVM 14 port
  └─port/llvm-XX.Y.Z  ← … one branch per target LLVM version
```

Each `port/llvm-*` branch is created from `master`, not from another version branch. This
allows agents to work on different versions in parallel.

## Workflow for a new port

1. Add `porting/llvm-releases/<version>/api-breakings.yaml` and `dockerfile-snippet.yaml` on `master`.
2. Create `port/llvm-<version>` from `master`.
3. Update `AGENTS.md` on that branch with the `## Current state` section.
4. Write task notes in `porting/TaskNotes/Tasks/` and have agents handle them.

If the target `port/llvm-<version>` branch doesn't exist, agents should create it from
`development`, or from `port/llvm-<previous_version>` as the first step.