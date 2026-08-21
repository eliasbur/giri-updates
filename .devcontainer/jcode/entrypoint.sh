#!/bin/bash
# Entrypoint: maps shared CHAT_TOKEN / CHAT_ENDPOINT to jcode's per-provider
# env files, then constructs base_url in config.toml at runtime.
# This mirrors how opencode.json uses {env:...} placeholders, since jcode's
# config.toml is static and cannot do runtime substitution.

set -euo pipefail

JCODE_CONFIG="/root/.config/jcode"
JCODE_ENV_DIR="$JCODE_CONFIG"
JCODE_CONFIG_LEGACY="/root/.jcode"
JCODE_SKILLS_DIR="/root/.jcode/skills"
JCODE_MEMORY_DIR="/root/.local/state/jcode/memory"
JCODE_FAILURES_DIR="/root/.jcode/failures"

# Download endorsed skills if not already present
download_endorsed_skills() {
  echo "[entrypoint] Downloading endorsed skills..."
  mkdir -p "$JCODE_SKILLS_DIR"
  
  # Download optimizations skill
  if [ ! -d "$JCODE_SKILLS_DIR/optimizations" ]; then
    echo "[entrypoint] Downloading optimizations skill..."
    mkdir -p "$JCODE_SKILLS_DIR/optimizations"
    # Try to fetch from Jcode's endorsed skills repository
    # Fallback: create minimal skill stub if download fails
    if ! curl -fsSL "https://raw.githubusercontent.com/1jehuang/jcode/main/skills/optimizations/SKILL.md" -o "$JCODE_SKILLS_DIR/optimizations/SKILL.md" 2>/dev/null; then
      cat > "$JCODE_SKILLS_DIR/optimizations/SKILL.md" <<'SKILL_EOF'
---
name: optimizations
description: Suggest and apply code optimizations for performance, readability, and maintainability.
---

# /optimize

Analyze code for optimization opportunities:
- Performance bottlenecks
- Code duplication
- Complex logic that can be simplified
- Better data structures or algorithms
- Readability improvements

Usage:
```
/optimize              # Analyze current directory
/optimize <path>       # Analyze specific path
/optimize <path> --apply  # Auto-apply safe optimizations
```
SKILL_EOF
    fi
  fi
  
  # Download todo-planning-skill
  if [ ! -d "$JCODE_SKILLS_DIR/todo-planning-skill" ]; then
    echo "[entrypoint] Downloading todo-planning-skill..."
    mkdir -p "$JCODE_SKILLS_DIR/todo-planning-skill"
    if ! curl -fsSL "https://raw.githubusercontent.com/1jehuang/jcode/main/skills/todo-planning-skill/SKILL.md" -o "$JCODE_SKILLS_DIR/todo-planning-skill/SKILL.md" 2>/dev/null; then
      cat > "$JCODE_SKILLS_DIR/todo-planning-skill/SKILL.md" <<'SKILL_EOF'
---
name: todo-planning-skill
description: Automatically break down complex tasks into actionable todo items with clear acceptance criteria.
---

# /plan

When given a task or goal, automatically create a structured plan:
1. Analyze the task requirements
2. Break down into discrete, testable steps
3. Create todo items with clear Definition of Done
4. Identify dependencies and parallelization opportunities

Usage:
```
/plan <task description>
/plan --file <task-file.md>
```
SKILL_EOF
    fi
  fi
  
  echo "[entrypoint] Endorsed skills ready in $JCODE_SKILLS_DIR"
}

# Index repository for active memory
index_repository() {
  local repo_path="${1:-.}"
  echo "[entrypoint] Indexing repository at $repo_path..."
  
  mkdir -p "$JCODE_MEMORY_DIR"
  
  # Create a project-specific memory index
  local repo_hash
  repo_hash=$(echo "$repo_path" | md5sum | cut -d' ' -f1)
  local memory_file="$JCODE_MEMORY_DIR/${repo_hash}.json"
  
  # Only index if not already indexed (or if --force)
  if [ -f "$memory_file" ] && [ "${FORCE_REINDEX:-}" != "true" ]; then
    echo "[entrypoint] Repository already indexed, skipping..."
    return 0
  fi
  
  # Check if we're in a git repository
  if command -v git &> /dev/null && git rev-parse --git-dir > /dev/null 2>&1; then
    echo "[entrypoint] Building repository index..."
    
    # Get list of files respecting .gitignore
    local files
    files=$(git ls-files 2>/dev/null || find . -type f -not -path '*/\.*' | head -500)
    
    # Create initial memory index
    cat > "$memory_file" <<INDEX_EOF
{
  "repo_path": "$repo_path",
  "indexed_at": "$(date -Iseconds)",
  "file_count": $(echo "$files" | wc -l),
  "files": $(echo "$files" | head -100 | jq -R . | jq -s .),
  "context": {
    "language_detected": [],
    "framework_detected": [],
    "architecture_notes": ""
  },
  "task_history": [],
  "failure_patterns": [],
  "learnings": []
}
INDEX_EOF
    
    echo "[entrypoint] Repository indexed: $memory_file"
  else
    echo "[entrypoint] Not a git repository, skipping index..."
  fi
}

if [ -n "$CHAT_TOKEN" ]; then
  # Write per-provider env files that jcode reads at startup.
  # jcode looks for providers.*.env_file under ~/.config/jcode/.
  mkdir -p "$JCODE_ENV_DIR"

  # chat-ai provider env file
  cat > "$JCODE_ENV_DIR/chat-ai.env" <<EOF
JCODE_PROVIDER_CHAT_AI_API_KEY=${CHAT_TOKEN}
EOF

  # saia provider env file
  cat > "$JCODE_ENV_DIR/saia.env" <<EOF
JCODE_PROVIDER_SAIA_API_KEY=${CHAT_TOKEN}
EOF
fi

if [ -n "$CHAT_ENDPOINT" ]; then
  # jcode's base_url is static config; we need to construct it at runtime.
  # CHAT_ENDPOINT is the host like "https://mythllm.mlsec.tu-berlin.de".
  # Strip trailing slashes from the endpoint prefix.
  ENDPOINT="${CHAT_ENDPOINT%/}"

  # Rewrite base_url in config.toml from placeholder to actual URL.
  # Apply to both ~/.config/jcode/ (standard) and ~/.jcode/ (legacy/jcode-reads-from-here)
  for CONFIG_DIR in "$JCODE_CONFIG" "$JCODE_CONFIG_LEGACY"; do
    if grep -q 'PLACEHOLDER_CHAT_AI' "$CONFIG_DIR/config.toml" 2>/dev/null; then
      sed -i \
        -e "s|PLACEHOLDER_CHAT_AI|${ENDPOINT}/chat-ai/v1|g" \
        -e "s|PLACEHOLDER_SAIA|${ENDPOINT}/saia/v1|g" \
        "$CONFIG_DIR/config.toml"
    fi
  done
fi

# Download endorsed skills
download_endorsed_skills

# Index repository if in a repo
if [ "${AUTO_INDEX_REPO:-true}" = "true" ]; then
  index_repository "${WORKSPACE_DIR:-.}"
fi

# Create failures directory structure
mkdir -p "$JCODE_FAILURES_DIR"
echo "[entrypoint] Failures directory ready: $JCODE_FAILURES_DIR"

exec "$@"