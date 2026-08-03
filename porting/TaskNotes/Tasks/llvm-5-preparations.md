---
title: Prepare the port to LLVM version 5.0.0.
repo: giriupdates
status: done
priority: medium # low | medium | high
contexts: [] # e.g. dev, cockpit, gpu1
projects: [ giriupdates ] # e.g. mythllm-client, irt-study
tags:
- task
timeEstimate: 0 # minutes
dateCreated: 2026-08-03
dateModified: 2026-08-03T12:00:50.486+02:00
completedDate: 2026-08-03
---
## Goal
Updated documents in the `porting/llvm-releases/5.0.0/` directory that document the changes necessary to port the Giri project for LLVM version 5.
  
## Definition of done
- [x] ~~Parsed already-present ReleaseNotes html file (see `porting/llvm-releases/5.0.0/LLVM 5.0.0 Release Notes.html`)~~
- [x] ~~Checked existing Giri code for parts that are affected by breaking changes in LLVM / Clang~~
- [x] ~~Constructed api-breakings.yaml, similar to the one presented in `porting/llvm-releases/8.0.0/`, based solely on the parsed release notes~~
- [x] ~~Constructed dockerfile-snippet.yaml, similar to the one presented in `porting/llvm-releases/8.0.0/`~~
- [x] ~~PR opened into `development` and linked below~~

## Files / scope
- `porting/llvm-releases/5.0.0/*`

## Handoff