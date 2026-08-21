#!/usr/bin/env bash
# Lightweight Docker availability probe — devcontainer postCreateCommand (warn-only).
# Project-agnostic: only checks socket connectivity and basic container execution.
# For full vd_pipeline validation, run agents/_lib/bin/probe-docker.sh manually.
set -euo pipefail

PASS=0
FAIL=0

check() {
  local name="$1"
  shift
  if "$@" >/dev/null 2>&1; then
    echo "[OK] $name"
    PASS=$((PASS + 1))
  else
    echo "[FAIL] $name"
    FAIL=$((FAIL + 1))
  fi
}

echo "=== Lightweight Docker probe ==="

check "docker version" docker version
check "busybox sibling" docker run --rm busybox echo hello

echo "=== Result: ${PASS} passed, ${FAIL} failed ==="
if [ "$FAIL" -gt 0 ]; then
  exit 1
fi