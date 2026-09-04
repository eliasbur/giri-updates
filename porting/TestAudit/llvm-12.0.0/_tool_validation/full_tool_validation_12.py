import os, re, subprocess, shutil, sys

GIRI = "/giri/test"
TRACER = "/giri/build/bin/tracer"
PRTRACE = "/giri/build/bin/prtrace"
BASE = "/root/fullval12"
BENCHES = ("matrix_multiply", "pca", "kmeans")
#
# s6 — standalone `tracer`/`prtrace` validation, LLVM 12.0.0 legacy-PM port.
#
# Honest scope (root-caused against this build):
#   The legacy-PM `tools/Tracer/Tracer.cpp` (original 3.4 tool, unchanged since
#   8.0.0 — see the 224bdfb..fba2565 delta) has a FIXED pipeline in main():
#     if (DoTrace)      { PM.add(TracingNoGiri);  PM.run(M); }   // -trace
#     else if (DynamicGiri) { PM.add(DynamicGiri); PM.run(M); }  // -slice
#   It does NOT add -bbnum/-lsnum (the `opt` harness does). DynamicGiri calls
#   QueryBasicBlockNumbers/QueryLoadStoreNumbers, which read the numbering maps
#   the -bbnum/-lsnum passes populate; with them absent, findPreviousID walks
#   invalid memory -> SIGBUS. So the standalone tracer's SLICE mode is not a
#   validatable path on this line (a pre-existing limitation, not a 12.0.0
#   regression; the slice result itself is validated via `opt` in the s5 suite).
#   The standalone tracer's INSTRUMENT stage (-trace, its real job) IS fully
#   validatable and is what this script runs over all 22 cases:
#     standalone `tracer -trace` instrument -> llc -> link -lrtgiri -> run the
#     real program (expected exit code, no Abnormal termination) -> the trace
#     file is non-empty and % 32 == 0 -> `prtrace` decodes it (End record =>
#     Entry ABI intact). On test1 this was cross-checked against the `opt`
#     harness's instrument stage: identical prtrace ID/Type sequence.
#
# 12.0.0 is pre-opaque-pointer (typed IR default) and the harness CFLAGS are
# `-g -O0 -c -emit-llvm` (no -Wno-error=implicit-function-declaration), so the
# new-PM-era 16.0.0 script's extra compile flags are dropped for faithfulness.

class R:
    pass

def run(cmd, cwd):
    r = subprocess.run(cmd, cwd=cwd, shell=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    o = R()
    o.returncode, o.stdout, o.stderr = r.returncode, r.stdout, r.stderr
    return o

def mkval(mkpath, key):
    for line in open(mkpath, encoding="utf-8"):
        m = re.match(r"^\s*%s\s*\??=\s*(.*)$" % key, line)
        if m:
            return m.group(1).strip()
    return ""

tests = [("UnitTests/test%d" % i, "UnitTests/test%d" % i)
         for i in [1,2,3,4,5,8,9,10,11,12,13,14,15,16,17,18,19,20,21]]
tests += [(d, d) for d in BENCHES]

results = []
for d, makedir in tests:
    mkpath = os.path.join(GIRI, makedir, "Makefile")
    bench = makedir in BENCHES
    name = mkval(mkpath, "NAME")
    if bench:
        name = makedir + "-seq"          # seq variant NAME
        srcs = [makedir + "-seq.c"]
        ld = ""                          # seq variant has no LDFLAGS
    else:
        srcs = [mkval(mkpath, "SRC_FILES")]
        if not srcs or not srcs[0]:
            srcs = sorted(f for f in os.listdir(os.path.join(GIRI, makedir))
                          if f.endswith(".c"))
        ld = mkval(mkpath, "LDFLAGS")
    inp = mkval(mkpath, "INPUT")
    ex = mkval(mkpath, "EXPECTED_EXIT")
    unc = mkval(mkpath, "EXIT_UNCHECKED")
    wd = os.path.join(BASE, name)
    if os.path.exists(wd):
        shutil.rmtree(wd)
    os.makedirs(wd)
    for f in os.listdir(os.path.join(GIRI, makedir)):
        if os.path.isfile(os.path.join(GIRI, makedir, f)):
            shutil.copy(os.path.join(GIRI, makedir, f), wd)
    tag = makedir.split("/")[-1] + "/" + name
    try:
        bc = []
        for src in sorted(srcs):
            r = run("clang -g -O0 -c -emit-llvm %s -o %s" % (src, src[:-3] + ".bc"), wd)
            if r.returncode != 0:
                raise Exception("compile %s: %s" % (src, r.stderr.decode("utf-8", "replace")[:150]))
            bc.append(src[:-3] + ".bc")
        if len(bc) > 1:
            r = run("llvm-link %s -o all.bc" % " ".join(bc), wd)
            if r.returncode != 0:
                raise Exception("llvm-link: %s" % r.stderr.decode("utf-8", "replace")[:150])
            prog = "all.bc"
        else:
            prog = bc[0]
        # 1. standalone tracer: instrument (-trace; the standalone-able stage)
        r = run("%s -trace -f -o instr.bc -trace-file=prog.trace %s" % (TRACER, prog), wd)
        if r.returncode != 0:
            raise Exception("instrument: %s" % r.stderr.decode("utf-8", "replace")[:150])
        # 2. llc + link (link -lrtgiri) + run the real program
        r = run("llc -asm-verbose=false -O0 instr.bc -o instr.s", wd)
        if r.returncode != 0:
            raise Exception("llc: %s" % r.stderr.decode("utf-8", "replace")[:150])
        r = run("clang++ -fno-strict-aliasing instr.s -o prog.exe -L/giri/build/lib -lrtgiri %s" % ld, wd)
        if r.returncode != 0:
            raise Exception("link: %s" % r.stderr.decode("utf-8", "replace")[:150])
        rc = run("./prog.exe %s >/dev/null 2>run.err" % inp, wd).returncode
        if b"Abnormal termination" in open(os.path.join(wd, "run.err"), "rb").read():
            raise Exception("abnormal termination (rc=%d)" % rc)
        if unc == "1":
            pass
        elif ex and ex != "-1":
            if rc != int(ex):
                raise Exception("exit %d != expected %s" % (rc, ex))
        else:
            if rc != 0:
                raise Exception("exit %d != 0" % rc)
        # 3. trace non-empty and Entry-ABI sized (%32 == 0)
        tsize = os.path.getsize(os.path.join(wd, "prog.trace"))
        if tsize <= 0:
            raise Exception("empty trace")
        if tsize % 32 != 0:
            raise Exception("trace size %d not %% 32" % tsize)
        # 4. prtrace decodes (End record present => Entry ABI intact)
        r = run("%s prog.trace 2>&1" % PRTRACE, wd)
        pt_ok = (r.returncode == 0 and b"End" in r.stdout)
        pt = "prtrace OK" if pt_ok else "prtrace FAIL rc=%d" % r.returncode
        results.append((tag, pt_ok,
                        "%s | run-exit=%d | trace=%dB(%%32=0)" % (pt, rc, tsize), ""))
    except Exception as e:
        results.append((tag, False, "FAIL: %s" % e, ""))

print("=" * 96)
np_ = sum(1 for r in results if r[1])
for tag, ok, detail, _ in results:
    print("%-30s %-6s %s" % (tag, "PASS" if ok else "FAIL", detail))
print("=" * 96)
print("standalone tracer/ prtrace INSTRUMENT result: %d PASS / %d FAIL (of %d)" % (np_, len(results) - np_, len(results)))
