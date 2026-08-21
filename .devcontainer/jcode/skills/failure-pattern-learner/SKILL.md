---
name: failure-pattern-learner
description: Learn from failures and mistakes by organizing them in active memory. Curate and update existing related memories to prevent recurrence and build institutional knowledge.
---

# /learn-failure

Extract learnings from failures, debug sessions, or mistakes. Store them in project memory with clear prevention strategies. Use after resolving any error, bug, or unexpected behavior.

## Usage

```
/learn-failure                                    # Learn from most recent failure in conversation
/learn-failure --context "<error message>"        # Provide specific error context
/learn-failure --task "<task-file.md>"            # Learn from failed task
/learn-failure --search "<query>"                 # Find related failure patterns
/learn-failure --list                             # List all stored failure patterns
```

## What You Must Do When Invoked

### Step 1 - Extract failure context

Gather complete information about the failure:

**From conversation** (if no args):
- Scroll back to find the error/failure
- Extract: error message, stack trace, commands run, expected vs actual behavior

**From `--context`**:
- Use provided error message as starting point
- Ask clarifying questions if context is incomplete

**From `--task`**:
- Read the task file
- Find failure details in Handoff or conversation history
- Extract what went wrong and how it was resolved

Required information:
- **Error message**: Exact text of the error
- **Context**: What were you trying to do?
- **Root cause**: Why did it fail? (after debugging)
- **Resolution**: How was it fixed?
- **Prevention**: How to avoid this in future?

If any of these are missing, ask the user or search conversation history.

### Step 2 - Search for related failure patterns

Before creating new memory, check for existing related patterns:

```bash
# Search memory for similar failures
jcode memory search --query "<key-terms-from-error>"
```

Look for:
- Same error message or error type
- Same component/system (e.g., "Slurm", "Apptainer", "network")
- Same root cause category (e.g., "permission", "timeout", "resource")

If related patterns exist:
- Read them to understand existing knowledge
- Determine if this is:
  - **Duplicate**: Same failure, skip or update existing
  - **Variation**: Related but distinct, link to existing
  - **New pattern**: Different root cause, create new entry

### Step 3 - Create failure pattern entry

Store the failure pattern in project memory:

```bash
jcode memory remember --category "fact" --scope "project" --tags "failure,<component>,<error-type>" --content '<structured-failure-json>'
```

Structure the content as JSON:

```json
{
  "type": "failure_pattern",
  "id": "<slug-from-error-name>",
  "created_at": "<ISO-timestamp>",
  "error": {
    "message": "<exact error message or pattern>",
    "type": "<PermissionError|TimeoutError|etc>",
    "component": "<Slurm|Apptainer|Network|etc>",
    "severity": "blocking|warning|nuisance"
  },
  "context": {
    "what_was_attempted": "<description of the action>",
    "environment": "<relevant env vars, system state>",
    "prerequisites": "<what must be true for this to work>"
  },
  "root_cause": {
    "summary": "<one-sentence explanation>",
    "detailed": "<full technical explanation>",
    "category": "<permission|resource|config|network|dependency|logic>"
  },
  "resolution": {
    "immediate_fix": "<what fixed it this time>",
    "commands": ["<exact commands to fix>"],
    "code_changes": ["<files modified if any>"]
  },
  "prevention": {
    "checklist": ["<things to verify before running>"],
    "automation": "<scripts or checks that could prevent this>",
    "documentation": "<where this should be documented>"
  },
  "related": {
    "tasks": ["<task files where this occurred>"],
    "memories": ["<IDs of related memories>"],
    "external_links": ["<relevant docs, issues, StackOverflow>"]
  },
  "confidence": "<high|medium|low>",
  "verified": false
}
```

### Step 4 - Create markdown file in `.devcontainer/jcode/failures/`

Also store a human-readable markdown file:

```bash
mkdir -p ".devcontainer/jcode/failures"
cat > ".devcontainer/jcode/failures/<slug>.md" << 'EOF'
---
created: <date>
component: <component>
error_type: <type>
severity: <severity>
status: resolved|monitoring|unresolved
---

# <Descriptive Title>

## Error Message

```
<exact error message>
```

## Context

What was being attempted when this failure occurred.

## Root Cause

Technical explanation of why this failed.

## Resolution

### Immediate Fix

Commands or changes that resolved the issue:

```bash
<commands>
```

### Code Changes

Files modified (if any):
- `path/to/file.py`: description of change

## Prevention

### Checklist

Before running <action>, verify:
- [ ] Check 1
- [ ] Check 2

### Automation

Scripts or checks that could prevent this:
```bash
<suggested pre-flight check>
```

## Related

- Tasks: <links to task files>
- Similar failures: <links to other failure docs>
- External: <documentation links>

## History

- <date>: First encountered and resolved
- <date>: Recurred, fix documented
EOF
```

### Step 5 - Link to active memory

Create bidirectional links:

1. **Tag the memory** with relevant identifiers:
```bash
jcode memory tag --id "<memory-id>" --tags "failure-pattern,<component>,<project-area>"
```

2. **Link related memories** if applicable:
```bash
jcode memory link --from "<new-failure-id>" --to "<related-memory-id>" --relation "similar_to"
```

### Step 6 - Surface relevant patterns proactively

After storing the failure, set up proactive surfacing:

1. **Create a trigger phrase list** - Extract key terms that should trigger surfacing this pattern:
   - Error message keywords
   - Command names
   - File paths
   - Component names

2. **Add to memory injection** - If Jcode supports memory injections, add this pattern to inject when:
   - User mentions similar error
   - Running same command that failed
   - Working in same file/directory

### Step 7 - Report to user

After storing the failure pattern, report:

```
✅ Failure pattern learned: <slug>

📁 Stored in:
  - Memory: <memory-id>
  - File: .devcontainer/jcode/failures/<slug>.md

🏷️ Tags: failure, <component>, <error-type>

🔍 This pattern will surface when:
  - Error message matches: "<key-terms>"
  - Running command: `<problematic-command>`
  - Working in: <affected-directory>

💡 Prevention checklist:
  - [ ] <prevention-item-1>
  - [ ] <prevention-item-2>

📊 Statistics:
  - Total failure patterns: <count>
  - This component: <component-count>
  - Most common: <top-failure-type>
```

## When to Use This Skill

Use failure-pattern-learner when:

1. **After debugging any error** - Once you've fixed it, document it
2. **Recurring issues** - Same error happens twice, create a pattern
3. **User asks "why did this fail?"** - After explaining, capture it
4. **Post-mortem on failed task** - Extract learnings from task failure
5. **Near-miss prevention** - Almost failed but caught it early

## Failure Pattern Categories

Categorize failures for better organization:

| Category | Examples |
|----------|----------|
| `permission` | Access denied, sudo required, file permissions |
| `resource` | OOM, disk full, GPU unavailable, quota exceeded |
| `network` | Connection timeout, DNS failure, SSL error |
| `config` | Missing env var, wrong path, invalid JSON/YAML |
| `dependency` | Version conflict, missing package, import error |
| `logic` | Off-by-one, null pointer, race condition |
| `environment` | Wrong Python version, missing tool, PATH issue |

## Quality Checklist

Before marking failure as learned:

- [ ] **Exact error captured**: Copy-pasted, not paraphrased
- [ ] **Root cause identified**: Not just symptoms
- [ ] **Reproducible steps**: Could someone else trigger it?
- [ ] **Fix verified**: Actually works, not just theorized
- [ ] **Prevention actionable**: Concrete steps, not vague advice
- [ ] **Related patterns linked**: Connected to existing knowledge
- [ ] **Markdown readable**: Human can understand without JSON parsing

## Examples

### Example 1: Apptainer permission error

```
/learn-failure --context "Apptainer build failed: permission denied on /tmp"
```

Creates pattern for:
- Error: `permission denied on /tmp`
- Component: Apptainer
- Root cause: `/tmp` mounted noexec
- Resolution: Use `$SINGULARITY_TMPDIR` in writable location
- Prevention: Check mount options before build

### Example 2: Slurm job timeout

```
/learn-failure --task docs/TaskNotes/Tasks/train-model.md
```

Extracts from task:
- Error: Job killed after 24h (time limit)
- Component: Slurm
- Root cause: `--time` not specified, defaulted to min
- Resolution: Set `--time=48:00:00` explicitly
- Prevention: Always specify `--time` in job scripts

### Example 3: Python import error

```
/learn-failure --context "ModuleNotFoundError: No module named 'graphify'"
```

Creates pattern:
- Error: `ModuleNotFoundError`
- Component: Python
- Root cause: Virtualenv not activated
- Resolution: `source .venv/bin/activate`
- Prevention: Check `sys.executable` matches expected venv

## Anti-Patterns

❌ **Blaming without understanding**: "It just failed" - dig deeper
❌ **No prevention**: Just documenting the fix isn't enough
❌ **Vague titles**: "Error occurred" - be specific
❌ **Isolated knowledge**: Not linking to related patterns
❌ **Unverified fixes**: Documenting what you think would work
❌ **Memory only**: Not creating human-readable markdown

## Maintenance

Periodically review failure patterns:

```bash
# List all patterns
/learn-failure --list

# Find patterns by component
/learn-failure --search "Apptainer"

# Mark patterns as verified (after recurrence testing)
# Edit markdown: status: verified
```

**Deprecate obsolete patterns** when:
- Root cause is fixed upstream
- Workaround is no longer needed
- Pattern was incorrect

Mark as `status: deprecated` with explanation.

## Integration with Other Systems

### Git Integration
- Link failure patterns to commits that fixed them
- Reference in commit messages: `Fixes failure:<slug>`

### Task Integration
- Link to `handle-task` failures in Handoff section
- Create tasks for recurring failures needing automation

### Memory System
- Use Jcode's memory search for retrieval
- Tag with project-specific identifiers
- Link to experiment documentation
