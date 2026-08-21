---
name: skill-factory
description: Create new custom skills following the Agent Skills specification. Use when you need to scaffold a new skill folder, generate a SKILL.md file, or help users understand the Agent Skills framework.
---

# /create-skill

Meta-skill for creating new Agent Skills. When invoked, scaffold a complete skill folder with all necessary files following the Agent Skills specification.

## Usage

```
/create-skill <skill-name> --description "<what it does>"
/create-skill <skill-name> --from-task "<task-file.md>"   # Create skill from completed task
/create-skill <skill-name> --after-failure "<context>"    # Create skill to prevent recurrence
```

## What You Must Do When Invoked

### Step 1 - Validate skill name

Skill names must be:
- Lowercase with hyphens (kebab-case)
- 2-40 characters
- Descriptive of the skill's purpose
- Not conflict with existing skills

Check for conflicts:
```bash
ls .claude/skills/ | grep -i "<skill-name>"
```

If conflict exists, suggest alternative name and stop.

### Step 2 - Create skill folder structure

```bash
mkdir -p ".claude/skills/<skill-name>"
```

### Step 3 - Generate SKILL.md

Create `.claude/skills/<skill-name>/SKILL.md` with this structure:

```markdown
---
name: <skill-name>
description: <Clear one-sentence description of what this skill does>
---

# /<command-trigger>

<One paragraph explaining when to use this skill and what problem it solves.>

## Usage

```bash
/<command> [arguments] [options]
/<command> --help
```

## What You Must Do When Invoked

<Numbered step-by-step instructions the agent must follow. Be specific and actionable.>

### Step 1 - <First action>

<Exact commands or actions to take. Include code blocks for commands.>

### Step 2 - <Second action>

<Continue with clear, sequential steps.>

## Examples

```bash
# Example 1: Basic usage
/<command> <example-input>

# Example 2: With options
/<command> <input> --option value
```

## Validation

Before considering the skill complete, verify:

- [ ] All required steps completed
- [ ] Output matches expected format
- [ ] Error cases handled gracefully
- [ ] User confirmed satisfaction

## Related Skills

- <link to related skill if applicable>
```

### Step 4 - Generate supporting files (if needed)

If the skill requires backend logic (like handle-task's driver.py):

1. Create `driver.py` with:
   - Argument parsing
   - Core logic functions
   - Error handling
   - Clear CLI interface

2. Create any helper scripts (e.g., `askpass.sh` for credentials)

3. Document dependencies in SKILL.md Setup section

### Step 5 - Create usage examples

Generate 2-3 concrete examples showing:
- Basic usage
- Advanced usage with options
- Edge case handling

### Step 6 - Validate the skill

Run this checklist:

```bash
# 1. Verify structure
ls -la ".claude/skills/<skill-name>/"

# 2. Check SKILL.md has required sections
grep -E "^---$|^name:|^description:|^# /" ".claude/skills/<skill-name>/SKILL.md"

# 3. Test any driver scripts
python3 ".claude/skills/<skill-name>/driver.py" --help 2>&1 | head -5
```

### Step 7 - Register with user

Tell the user:
1. Skill location: `.claude/skills/<skill-name>/`
2. How to invoke: `/<command-trigger>`
3. Any setup required (env vars, dependencies)
4. Offer to test it immediately

## When to Use This Skill

Use skill-factory when:

1. **After completing a complex task** - If you solved a non-trivial problem that might recur, suggest creating a skill to codify the solution.

2. **After resolving a failure** - When you've debugged and fixed an issue, create a skill to prevent recurrence and help with similar failures.

3. **User requests automation** - When user asks "can you always do X?" or "I wish you could Y automatically."

4. **Pattern recognition** - When you notice yourself doing the same multi-step process repeatedly.

## Skill Quality Checklist

Before finalizing any skill, ensure:

- [ ] **Clear trigger**: Command name is intuitive (`/<verb>-<noun>` format)
- [ ] **Single responsibility**: Skill does one thing well
- [ ] **Actionable steps**: Instructions are specific, not vague
- [ ] **Error handling**: Covers common failure modes
- [ ] **Validation**: Clear criteria for completion
- [ ] **Examples**: Realistic usage scenarios included
- [ ] **No hardcoding**: Uses parameters, not specific paths/names
- [ ] **Tested**: You've mentally walked through the steps

## Template Variables

When generating SKILL.md, replace:

- `<skill-name>` → kebab-case name (e.g., `api-client-generator`)
- `<command-trigger>` → slash command (e.g., `/generate-api-client`)
- `<description>` → one sentence, active voice
- `<what it does>` → problem it solves + outcome
- `<arguments>` → required inputs
- `<options>` → optional flags with defaults

## Anti-Patterns to Avoid

❌ **Too broad**: "Do all the things" - Skills should be focused
❌ **Too vague**: "Make it work" - Steps must be actionable
❌ **Hardcoded paths**: Use parameters and environment variables
❌ **No validation**: Always include completion criteria
❌ **No examples**: Users need concrete usage patterns
❌ **Hidden dependencies**: Document all requirements upfront

## Post-Creation: Test the Skill

After creating a skill, immediately test it:

```bash
# Switch to a test directory or create a sandbox
mkdir -p /tmp/skill-test && cd /tmp/skill-test

# Invoke the skill with test inputs
/<command> <test-input>

# Verify output matches expectations
```

If the skill modifies files, ensure it:
- Only touches intended files
- Creates backups if destructive
- Reports changes clearly

## Integration with Other Skills

Created skills should be:
- **Independent**: No dependencies on other custom skills
- **Composable**: Can be used in sequence with other skills
- **Non-interfering**: Don't modify other skills' outputs

Exception: Skills may use built-in Jcode features (memory, websearch, etc.)

## Examples

### Example 1: Create a skill from a completed task

After implementing a complex feature (e.g., "Add Slurm job monitoring"):

```
/create-skill slurm-job-monitor --from-task docs/TaskNotes/Tasks/add-slurm-monitoring.md
```

This analyzes the task note and extracts:
- Problem solved
- Steps taken
- Commands used
- Validation performed

### Example 2: Create a skill after fixing a bug

After debugging a recurring issue (e.g., "Apptainer build fails with permission error"):

```
/create-skill apptainer-permission-fixer --after-failure "Apptainer build failed: permission denied on /tmp"
```

This creates a skill that:
- Diagnoses the specific error
- Applies the verified fix
- Prevents recurrence

### Example 3: Create a utility skill

```
/create-skill python-venv-setup --description "Set up isolated Python virtual environment for research projects"
```

Generates a skill that:
- Creates `.venv/` with project-specific Python version
- Installs common research dependencies
- Configures VS Code integration
- Documents activation/deactivation

## Output Format

After creating a skill, output this summary:

```
✅ Skill created: <skill-name>

📁 Location: .claude/skills/<skill-name>/
📄 Files:
  - SKILL.md (main skill definition)
  - driver.py (if applicable)
  - <other supporting files>

🚀 Usage: /<command-trigger> [arguments]

📋 Next steps:
1. Review the generated SKILL.md
2. Test with: /<command> <test-input>
3. Customize if needed (edit SKILL.md directly)

💡 Tip: Skills are loaded automatically on next session. No restart needed.
```
