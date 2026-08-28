import os, re, subprocess, shutil

GIRI = "/giri/test"
TRACER = "/giri/build/bin/tracer"
PRTRACE = "/giri/build/bin/prtrace"
BASE = "/root/fullval"
BENCHES = ("matrix_multiply", "pca", "kmeans")

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

def nums(text):
    """harness goldens + the awk slice extraction are bare line numbers per line"""
    return sorted({l.strip() for l in text.splitlines() if l.strip().isdigit()}, key=float)

tests = [("UnitTests/test%d" % i, "UnitTests/test%d" % i)
         for i in [1,2,3,4,5,8,9,10,11,12,13,14,15,16,17,18,19,20,21]]
tests += [(d, d) for d in BENCHES]

results = []
for d, makedir in tests:
    mkpath = os.path.join(GIRI, makedir, "Makefile")
    bench = makedir in BENCHES
    name = "hello-world" if makedir == "HelloWorld" else mkval(mkpath, "NAME")
    if bench:
        name = makedir + "-seq"
    srcs = [mkval(mkpath, "SRC_FILES")]
    if not srcs or not srcs[0]:
        srcs = sorted(f for f in os.listdir(os.path.join(GIRI, makedir)) if f.endswith(".c"))
    if bench:
        srcs = [makedir + "-seq.c"]
    inp = mkval(mkpath, "INPUT")
    ex = mkval(mkpath, "EXPECTED_EXIT")
    unc = mkval(mkpath, "EXIT_UNCHECKED")
    crit = mkval(mkpath, "CRITERION")
    ld = mkval(mkpath, "LDFLAGS")
    ansname = mkval(mkpath, "TEST_ANS")
    if bench:
        crit = "-criterion-inst=criterion-inst-seq.txt"
        ld = ""
        ansname = "ans-inst-seq.txt"
    ans = os.path.join(GIRI, makedir, ansname if ansname else "ans-inst.txt")
    wd = os.path.join(BASE, name)
    if os.path.exists(wd):
        shutil.rmtree(wd)
    os.makedirs(wd)
    for f in os.listdir(os.path.join(GIRI, makedir)):
        if os.path.isfile(os.path.join(GIRI, makedir, f)):
            shutil.copy(os.path.join(GIRI, makedir, f), wd)
    tag = (makedir.split("/")[-1] + "/" + name)
    try:
        bc = []
        for src in sorted(srcs):
            r = run("clang -g -O0 -c -emit-llvm -Wno-error=implicit-function-declaration -Xclang -no-opaque-pointers %s -o %s" % (src, src[:-3] + ".bc"), wd)
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
        # 1. standalone tracer: instrument
        r = run("%s -trace -f -o instr.bc -trace-file=prog.trace %s" % (TRACER, prog), wd)
        if r.returncode != 0:
            raise Exception("instrument: %s" % r.stderr.decode("utf-8", "replace")[:150])
        # 2. llc + link + run
        r = run("llc -asm-verbose=false -O0 instr.bc -o instr.s", wd)
        if r.returncode != 0:
            raise Exception("llc: %s" % r.stderr.decode("utf-8", "replace")[:150])
        r = run("clang++ -fno-strict-aliasing -no-pie instr.s -o prog.exe -L/giri/build/lib -lrtgiri %s" % ld, wd)
        if r.returncode != 0:
            raise Exception("link: %s" % r.stderr.decode("utf-8", "replace")[:150])
        rc = run("./prog.exe %s >/dev/null 2>run.err" % inp, wd).returncode
        if b"Abnormal termination" in open(os.path.join(wd, "run.err"), "rb").read():
            raise Exception("abnormal termination")
        if unc == "1":
            pass
        elif ex and ex != "-1":
            if rc != int(ex):
                raise Exception("exit %d != expected %s" % (rc, ex))
        else:
            if rc != 0:
                raise Exception("exit %d != 0" % rc)
        # 3. trace non-empty (the negative control already proved empty can't match)
        if os.path.getsize(os.path.join(wd, "prog.trace")) <= 0:
            raise Exception("empty trace")
        # 4. standalone tracer: slice
        r = run("%s -f -trace-file=prog.trace -slice-file=slice.txt %s %s" % (TRACER, crit, prog), wd)
        if r.returncode != 0:
            raise Exception("slice: %s" % r.stderr.decode("utf-8", "replace")[:200])
        # 5. compare vs golden (bare numbers, like the harness's slice.loc)
        r = run("awk -F: '$0 ~ /^Source Line Info/ && $NF ~ /^[0-9]+$/ {print $NF}' slice.txt | sort -g | uniq", wd)
        got = nums(r.stdout.decode("utf-8", "replace"))
        want = nums(open(ans, encoding="utf-8").read())
        if not want:
            results.append((tag, False, "GOLDEN EMPTY (%s) - INVALID CHECK" % ans, ""))
            continue
        ok = got == want
        res = "MATCH (%d locs == golden)" % len(got) if ok else "MISMATCH got=%s want=%s" % (got[:6], want[:6])
        # 6. prtrace decode (public reader, Entry ABI)
        r = run("%s prog.trace 2>&1" % PRTRACE, wd)
        pt_ok = (r.returncode == 0 and b"End" in r.stdout)
        pt = "prtrace OK" if pt_ok else "prtrace FAIL rc=%d" % r.returncode
        results.append((tag, ok and pt_ok, "%s | %s | run-exit=%d | trace=%dB" % (res, pt, rc, os.path.getsize(os.path.join(wd, "prog.trace"))), ""))
    except Exception as e:
        results.append((tag, False, "FAIL: %s" % e, ""))

print("=" * 96)
np_ = sum(1 for r in results if r[1])
for tag, ok, detail, _ in results:
    print("%-34s %-6s %s" % (tag, "PASS" if ok else "FAIL", detail))
print("=" * 96)
print("standalone-tracer FULL RESULT: %d PASS / %d FAIL (of %d)" % (np_, len(results) - np_, len(results)))
