---
title: Prepare the port to LLVM version 5.0.0.
repo: giriupdates
status: open
priority: medium # low | medium | high
contexts: [] # e.g. dev, cockpit, gpu1
projects: [ giriupdates ] # e.g. mythllm-client, irt-study
tags:
- task
timeEstimate: 0 # minutes
dateCreated: 2026-07-31
---
## Goal
Updated documents in the `porting/llvm-releases/5.0.0/` directory that document the changes necessary to port the Giri project for LLVM version 5.
  
## Definition of done
- [ ] Parsed ReleaseNotes html file for the requested version
- [ ] Constructed api-breakings.yaml, similar to the one presented in `porting/llvm-releases/8.0.0/`
- [ ] Constructed dockerfile-snippet.yaml, similar to the one presented in `porting/llvm-releases/8.0.0/`
- [ ] PR opened into `development` and linked below

## Files / scope
- `porting/llvm-releases/5.0.0/*`

## Handoff