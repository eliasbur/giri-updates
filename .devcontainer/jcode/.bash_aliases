# Symlink shared skills from .devcontainer into workspace .claude directory
# so agents can discover them and driver.py's parents[3] resolves correctly.
if [ -d "$(pwd)/.devcontainer/skills" ] && [ ! -L ".claude/skills" ]; then
  mkdir -p .claude && ln -sfn "$(pwd)/.devcontainer/skills" .claude/skills
fi
