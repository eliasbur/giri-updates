#!/bin/sh
# GIT_ASKPASS helper for handle-task's driver.py.
# Reads the token from GIT_ASKPASS_TOKEN (env, not argv) so it never
# appears in `ps` output on this shared host.
case "$1" in
  Username*) echo oauth2 ;;
  *) echo "$GIT_ASKPASS_TOKEN" ;;
esac
