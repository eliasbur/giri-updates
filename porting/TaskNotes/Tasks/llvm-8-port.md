---
title: Port Giri to LLVM 8.0.0.
status: open             # open | done (driver rewrites this on `finish`)
priority: high           # low | medium | high
repo: giriupdates        # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []             # e.g. dev, cockpit, gpu1
projects: [giriupdates]  # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0          # minutes
dateCreated: 2026-08-24
# dateModified / completedDate are added automatically by `driver.py finish`
---
## Goal
A Giri source tree on `port/llvm-8.0.0` that builds cleanly against LLVM/Clang 8.0.0 inside its
Docker image and runs the full test suite on the honest harness, with every remaining test
failure root-caused and documented, and the three critical invariants preserved.

## Approach

Work in four phases, strictly in this order. Do not start phase 1 before the phase 0 spike
resolves the toolchain question, and do not start phase 3 before you have a real compiler
error list from phase 2.

### Phase 0 — toolchain risk spike (gate everything)

The base image must move off `ubuntu:14.04`. LLVM 8.0.0 requires GCC ≥ 5.1 and C++14 to build,
and our CMake flow (`include(HandleLLVMOptions)`) propagates the C++ standard from
`LLVMConfig.cmake` into the consumer build, so our passes will be compiled as C++14.
`ubuntu:14.04` ships gcc 4.8 (C++11 only) and has no gcc-5 in its repos.

- **Recommended option:** `FROM ubuntu:16.04` (ships gcc 5.4, meets the 8.0.0 minimum) with the
  prebuilt `clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz` (verified to exist on
  releases.llvm.org, ~340 MB). Keep the CMake binary-tarball pin (cmake 3.12.4) rather than
  16.04's apt cmake 3.5.
- Alternatives, in order of preference: `ubuntu:18.04` + its 8.0.0 tarball (gcc 7.4, even more
  headroom); keep 14.04 and add a newer GCC via PPA/binary tarball (rejected by default —
  brittle).
- **Spike:** build the candidate image, then compile a throwaway hello-world `opt` pass against
  the 8.0.0 headers using the same flags our tree uses (`HandleLLVMOptions`,
  `-fno-rtti -fno-exceptions` in non-Debug). If the chosen gcc fails on C++14 header features,
  drop one rung (16.04 → 18.04) and re-spike.
- **Gate:** the hello-world pass compiles, links, and loads in `opt` 8.0.0. Record the chosen
  base image + gcc version in the Progress log before touching anything else.

### Phase 1 — toolchain + build system

- Add an `"8.0.0"` case to `utils/install_llvm.sh` that downloads the **prebuilt** tarball for
  the distro chosen in phase 0 from `https://releases.llvm.org/8.0.0/` and unpacks it to
  `$LLVM_HOME`. Skip the autoconf tail (the prebuilt tarball has none — same shape as the 5.0.2
  case). Keep the `3.4` and `5.0.2` cases intact (3.4 for original-toolchain reproducibility,
  5.0.2 for the sibling branch's image).
- `Dockerfile`: apply the phase 0 base-image decision; keep `ENV LLVM_HOME=/usr/local/llvm`,
  `BuildMode=Release+Asserts`, `TEST_PARALLELISM=seq`, and the PATH line; switch the install
  call to `RUN giri/utils/install_llvm.sh 8.0.0`.
- Top-level `CMakeLists.txt`: `find_package(LLVM 8.0 REQUIRED CONFIG)`. Our libraries use plain
  `add_library(... SHARED)` — **not** `add_llvm_loadable_module` (which LLVM 8 removed) and not
  `add_llvm_library(... MODULE)` — so the 8.0.0 release-notes CMake removal does not apply.
  Do not "fix" CMake structure that compiles; change only what the version bump requires.
- Per-directory `CMakeLists.txt` files should stay untouched unless the compiler demands it.
- `runtime/Giri/Tracing.cpp` (`librtgiri`) has **no LLVM dependency** — it must not pick up
  LLVM's flags beyond C++/pthreads.
- Output layout stays the flat `build/{lib,bin}` established on 5.0.2
  (`test/Makefile.common` and `test/HelloWorld/Makefile` resolve it).
- **Gate:** `docker build -t giri-llvm-8 .` succeeds; `source /giri/utils/build.sh` produces
  `build/lib/{libgiri.so,libdgutility.so,librtgiri.a}` and `build/bin/{tracer,prtrace}`;
  `opt -load build/lib/libdgutility.so -load build/lib/libgiri.so -help` lists
  `-bbnum`, `-lsnum`, `-trace-giri`, `-dgiri`.

### Phase 2 — iteratively fix the Giri sources

Work error-by-error, rebuilding after each fix; collect the full compiler error list first.

Before starting, author the structured change data for this range (the same discipline the 5.0.2
port followed): `porting/llvm-releases/8.0.0/{6.0.0,7.0.0,8.0.0}-api-breakings.yaml` plus a
consolidated `porting/llvm-releases/8.0.0/api-breakings.yaml`, from the LLVM 6/7/8 release-notes
HTML, using the schema of `porting/llvm-releases/5.0.0/api-breakings.yaml`
(`.release-notes-changes-template.yaml` + `versions`/`originalIds`). For every entry you act
on, set `relevance` to `affected`/`unlikely`/`irrelevant` and `status` to
`addressed`/`mitigated`/`skipped` as you go. That file is part of the deliverable.

Known hazards in the 5.0.2 tree (verified by grep before this note was written; confirm against
actual compiler output — do not pre-emptively rewrite code that still compiles):

- **`CallSite` / `llvm/IR/CallSite.h` — removed in 8.0.** Exactly four sites in the 5.0.2 tree:
  `lib/Giri/TraceFile.cpp:21` (include) and `:674` (`const CallSite CS(I);`),
  `lib/Giri/TracingNoGiri.cpp:28` (include), `lib/Utility/SourceLineMapping.cpp:17` (include).
  Migrate to `CallInst`/`User`/`Instruction` accessors (`getArgument(i)`, `getNumOperands()`,
  `getCalledFunction()`, …). `TraceFile.cpp` is the most subtle file in the repo — be careful.
- **Debug-info creators** — `DISubprogram::create`/`DILocation::get`/`DIDescriptor::get`
  signatures gained a `DITemplateParameterArray` argument in 8.0. Grep found **no direct
  creators** in the 5.0.2 tree, so expect zero fixes here — but `lib/Utility/SourceLineMapping.cpp`
  *reads* DI metadata; verify it still yields correct `file:line` mappings (invariant 3 makes
  `-g` non-negotiable).
- **Alignment APIs** — `Type::getAlignment` family is deprecated in 8.0; grep found **zero** hits
  in `lib/`, `tools/`, `runtime/`. Low risk.
- Everything in the 3.4→5.0.2 consolidated change log (388 entries) was already `addressed` on
  `port/llvm-5.0.2`; the new delta for this port is **6.0.0/7.0.0/8.0.0 only**.

The three invariants in `AGENTS.md` ("Critical invariants") must hold after the port:
numbering determinism (`-bbnum`/`-lsnum` produce identical IDs across the instrumentation and
slicing runs), the `Entry` struct ABI in `include/Giri/Runtime.h` (layout **and** the
size-divides-page-size property), and debug-info-driven source line mapping. If a fix would
change any of them, stop and write down why in the PR description instead of silently changing it.

**Gate:** zero compile errors against 8.0.0; both passes load in `opt` 8.0.0.

### Phase 3 — test suite, audit, invariants

- Run the suite (`make -C /giri/test`) on the honest harness (per-test `EXPECTED_EXIT`/
  `EXIT_UNCHECKED` + crash detection, as established on 5.0.2).
- Baseline expectation: the 5.0.2 port ended at **22 PASS / 0 FAIL** (with
  `matrix_multiply-seq` at a documented `FAIL-EXPECTED` criterion drift, criterion 292). LLVM
  8.0.0 emits different code, so expect new golden-file criterion drift and possibly new
  per-test diffs. A mismatch is **not** allowed to be "fixed" by regenerating the golden file
  unless you can explain the difference. Every remaining failure needs a one-paragraph root
  cause; write per-test reports under `porting/TestAudit/llvm-8.0.0/` mirroring the
  `porting/TestAudit/llvm-5.0.2/` structure (SUMMARY.md + per-test files + a
  "Suite results across the port" table).
- Re-verify the three invariants; record the verification in the TestAudit summary.
- Add a `## Current state` section to `AGENTS.md` on this branch, matching the new flow.

## Definition of done
- [ ] `port/llvm-8.0.0` exists, created from `port/llvm-5.0.2` (see Notes on the deprecated remote branch), with the base commit recorded in the Progress log
- [ ] Phase 0 spike result (chosen base image + gcc version + hello-world pass proof) recorded in the Progress log
- [ ] `utils/install_llvm.sh` gained a working `8.0.0` case using the prebuilt release tarball, with the `3.4` and `5.0.2` cases unchanged
- [ ] `Dockerfile` builds end to end for LLVM 8.0.0 (`docker build -t giri-llvm-8 .`), with 8.0.0 tools on `PATH`
- [ ] CMake pins `find_package(LLVM 8.0 REQUIRED CONFIG)`; build produces `libgiri.so`, `libdgutility.so`, `librtgiri.a`, `tracer`, `prtrace` in the established flat `build/{lib,bin}` layout
- [ ] Both passes load in `opt` 8.0.0 (`opt -load build/lib/libdgutility.so -load build/lib/libgiri.so -help` lists `-bbnum`, `-lsnum`, `-trace-giri`, `-dgiri`)
- [ ] Giri sources compile with zero errors against 8.0.0
- [ ] Full suite executed on the honest harness; pass/fail per test recorded, and each remaining failure root-caused (reports under `porting/TestAudit/llvm-8.0.0/`)
- [ ] `porting/llvm-releases/8.0.0/api-breakings.yaml` (+ per-version files) exists; every entry touched has `relevance` and `status` updated
- [ ] The three invariants in `AGENTS.md` verified or their deviation explained in the PR
- [ ] `AGENTS.md` on `port/llvm-8.0.0` gained a `## Current state` section, and its build/test commands match the CMake flow
- [ ] PR opened into `port/llvm-8.0.0` and linked below

## Files / scope
- `Dockerfile`
- `utils/install_llvm.sh`
- `CMakeLists.txt` (top-level; per-directory `CMakeLists.txt` only if forced)
- `lib/Giri/TraceFile.cpp`, `lib/Giri/TracingNoGiri.cpp`, `lib/Utility/SourceLineMapping.cpp` (known `CallSite` sites; the full set is defined by the compiler)
- `porting/llvm-releases/8.0.0/` (new), `porting/TestAudit/llvm-8.0.0/` (new)
- `AGENTS.md` (branch copy: `## Current state`)
- Do **not** change `include/Giri/Runtime.h`'s `Entry` layout, do not edit `test/**/ans-*.txt` without a written justification, and do not touch the `3.4` case of `utils/install_llvm.sh`.

## Notes
- **Two containers.** `driver.py`, git and the source tree live in the agent devcontainer.
  Building and testing Giri happens **only** inside the Giri Docker container
  (`docker build -t giri-llvm-8 .` then `docker run -it --rm -v $PWD:/giri giri-llvm-8 bash`).
  Never run `build.sh` or `make -C test` in the devcontainer. See `AGENTS.md` → "Containers —
  two kinds".
- Iterating through phase 2/3 by rebuilding the whole image each time is far too slow. Build
  the image once, then mount the working tree into a long-lived container and rebuild in place
  (the 5.0.2 port's main iteration tip).
- **Target branch: `port/llvm-8.0.0`, created from `port/llvm-5.0.2`.** `porting/README.md`
  names version branches as "created from `master`", but its workflow section also says agents
  "should create it from `development`, or from `port/llvm-<previous_version>` as the first
  step" — and `master`/`development` are still the raw 3.4 autoconf trees (no CMake), so a
  from-`master` 8.0.0 port would redo the entire 5.0.2 build-system + harness work. Branching
  from the completed 5.0.2 port is the sanctioned fallback and the decision the user endorsed
  (2026-08-24). Record the exact base commit in the Progress log.
- **Set `TARGET_BRANCH=port/llvm-8.0.0` in the shell before running the driver.** The
  `handle-task` driver reads the target branch **only** from the `TARGET_BRANCH` env var
  (defaulting to `development`); it does not read it from the note. Without this export,
  `resolve` reports `target_branch: development` and skill step 5 (`git checkout -b <branch>
  <target_branch>`) would cut the working branch from the raw 3.4 tree — the exact base this
  note forbids. The rehearsal pickup (2026-08-24) flagged this. Export it for the whole
  session, and also pass `--target port/llvm-8.0.0` to `open-mr` (the flag exists so the MR
  target is correct even if the env var is lost).
- **Token is available in this environment.** `.devcontainer/jcode/.env` (gitignored,
  `.gitignore:28`) defines `GITHUB_GIRIUPDATES_TOKEN` + `GITHUB_GIRIUPDATES_PATH` (GitHub
  backend, repo `eliasbur/giri-updates`) and `AGENT_BRANCH_PREFIX`. It is **not** auto-exported
  into the shell, so source it first: `set -a; . ./.devcontainer/jcode/.env; set +a`. Verified
  live 2026-08-24: the token authenticates as `eliasbur`; push works through the driver's
  askpass mechanism (token stays in env) and `gh pr create` opened a throwaway PR (#17, closed,
  branch deleted). A pickup session therefore needs no credentials setup beyond sourcing that
  file — and **must** export `TARGET_BRANCH=port/llvm-8.0.0` in the same shell.
- **The remote still carries a deprecated `port/llvm-8.0.0`** (identical to
  `origin/deprecated/port/llvm-8.0.0`, commit `6088dc6` — an old scaffold). User decision
  (2026-08-24): ignore both. Before pushing the real work, the remote `port/llvm-8.0.0` must be
  replaced with the branch cut from `port/llvm-5.0.2`; that is a force-push of a remote branch,
  so obtain explicit user confirmation in the executing session and record it in the Progress
  log. Do not use the old scaffold as base, target, or reference.
- Driver defaults to `development` for the MR target, so pass the target explicitly at pickup:
  `driver.py open-mr porting/TaskNotes/Tasks/llvm-8-port.md --target port/llvm-8.0.0 ...`.
- Reference material: `porting/TaskNotes/Tasks/llvm-5-port.md` (the previous port's note — the
  model for phases and discipline), `porting/llvm-releases/5.0.0/api-breakings.yaml` (schema +
  status-tracking exemplar), `porting/TestAudit/llvm-5.0.2/SUMMARY.md` (audit + "Suite results
  across the port" exemplar), `porting/AgentGuide.md`, `porting/HowItWorks.md`, and the LLVM
  6.0.0/7.0.0/8.0.0 release notes.

## Blocked by
- ~~none~~

## Progress log
- 2026-08-24 `40a322a` — Setup: created `port/llvm-8.0.0` at the 5.0.2 tip `5527588` and published
  it (fast-forward, new branch). Note: the deprecated remote `port/llvm-8.0.0` (scaffold `6088dc6`)
  was deleted from the remote before this run started, so the force-push step in the plan was moot;
  the scaffold remains preserved at `origin/deprecated/port/llvm-8.0.0`. Cut working branch
  `agent/open-code/llvm-8-port`; brought the task note onto the port branch (it lives on
  `development`, not on the 5.0.2 base). next: Phase 0 — `install_llvm.sh` 8.0.0 case,
  `Dockerfile` ubuntu:16.04 + `.dockerignore`, image build, hello-world spike.
- 2026-08-24 — Fresh-agent pickup rehearsal (context-free swarm worker, no token, dry run):
  verdict "pickable from the note alone". `resolve` correctly fails token-less; the step-2
  resume check exits 128 (branch absent = normal fresh start); all Phase-2 hazard lines verified
  against `origin/port/llvm-5.0.2`; the deprecated remote branch confirmed at `6088dc6`.
  Two gaps fixed inline in this note: (1) the Refs path `porting/TestAudit/llvm-5.0.2/SUMMARY.md`
  does not exist on `development` — it lives on `origin/port/llvm-5.0.2`, now annotated in the
  Refs line; (2) the driver reads the target branch only from the `TARGET_BRANCH` env var, so
  following skill step 5 verbatim would cut from `development` — a Notes bullet now requires
  exporting `TARGET_BRANCH=port/llvm-8.0.0` before driver use. next: a token-holding session
  exports that variable and runs `handle-task llvm-8-port`.
- 2026-08-24 — Token smoke test (this environment): `.devcontainer/jcode/.env` carries
  `GITHUB_GIRIUPDATES_TOKEN` (identity `eliasbur`) + `GITHUB_GIRIUPDATES_PATH` +
  `AGENT_BRANCH_PREFIX`; it is gitignored and must be sourced explicitly. Full credential
  round-trip proven with a throwaway PR — push via the driver's askpass mechanism,
  `gh pr create` → eliasbur/giri-updates#17 (OPEN), then closed and the branch deleted
  (remote + local). No repo content changed. next: `handle-task llvm-8-port` can run
  end-to-end in this environment (token + `TARGET_BRANCH` export + the one user-confirmed
  force-push of the deprecated remote `port/llvm-8.0.0`).

## Handoff
- branch `agent/open-code/llvm-8-port`
- (MR/PR line is written by `driver.py finish`: `- {PR|MR}: <label> {#|!}<iid> <url>`)
Refs: `AGENTS.md`, `porting/README.md`, `porting/AgentGuide.md`, `porting/HowItWorks.md`,
`porting/TaskNotes/Tasks/llvm-5-port.md`, `porting/llvm-releases/5.0.0/api-breakings.yaml` (all
on `development`). **Branch-dependent refs — on `origin/port/llvm-5.0.2`, not on `development`**
(read them with `git show origin/port/llvm-5.0.2:<path>` before the branch is cut, then directly
afterward): `porting/TestAudit/llvm-5.0.2/SUMMARY.md` (audit + "Suite results across the port"
exemplar) and the `## Current state` section of the 5.0.2 `AGENTS.md`.
