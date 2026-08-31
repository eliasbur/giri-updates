# Slice result — ffmpeg SAMI fuzzer (ARVO 42473917)

Container `arvo-42473917-vul` (image `arvo-vscode:42473917-vul`), bug:
heap-buffer-overflow read at `libavcodec/htmlsubtitles.c:174` (ASan frame #1).

Produced by [`giri-arvo`](giri-arvo) with no criterion given and no
`GIRI_LINKCMD`:

```
giri-arvo arvo-42473917-vul --out ./slice \
  --build-sh-sed 's/for c in $CONDITIONALS/for c in ${GIRI_CODECS:-$CONDITIONALS}/' \
  --env GIRI_CODECS=SAMI
```

    criterion : ff_htmlmarkup_to_ass:571   (%503 = call i32 @strncmp(...))
                chosen automatically, ranked first of 8 candidates
    link line : found via the .giri_link section, after build.sh renamed
                tools/target_dec_sami_fuzzer to ffmpeg_AV_CODEC_ID_SAMI_fuzzer
    trace     : 28,776,832 bytes, 899,276 records, terminated
    module    : 91 TUs, 5,013,088 bytes .all.bc, 0 bitcode failures over 112 links

**Complete run** (exit 0):

| | |
|---|---|
| distinct source lines | 284 |
| slice file | 56,743,768 bytes (terse) |
| trace records | 420,184 load · 258,526 basic block · 140,162 store · 40,199 call · 40,199 return · 5 select · 1 end |

The `.slice.loc` digest is **byte-identical** (`md5 578c3d5f…`) to the earlier
hand-driven run that chose the criterion and the link line by hand. That is the
result this automation had to reproduce, and the comparison is the reason the
earlier digest was kept.

Without `-slice-terse` the same slice never finished: 50 minutes produced
256,936 of the 608,164 values (~42 %) and was still going. The source-line
digest is byte-identical in both modes — verified against that run and against
the `test2` golden.

## Distinct source lines per file

| lines | file |
|------:|------|
| 42 | `/src/ffmpeg/libavcodec/utils.c` |
| 38 | `/src/ffmpeg/libavutil/bprint.c` |
| 33 | `/src/ffmpeg/libavcodec/samidec.c` |
| 29 | `/src/ffmpeg/libavcodec/htmlsubtitles.c` |
| 27 | `/src/ffmpeg/libavutil/opt.c` |
| 25 | `/src/ffmpeg/libavutil/avstring.c` |
| 22 | `/src/ffmpeg/tools/target_dec_fuzzer.c` |
| 18 | `/src/ffmpeg/libavutil/mem.c` |
| 14 | `/src/ffmpeg/libavcodec/avpacket.c` |
| 7 | `/src//giri/standalone_driver.c` |
| 7 | `/src/ffmpeg/libavcodec/options.c` |
| 6 | `/src/ffmpeg/libavcodec/decode.c` |
| 4 | `/src/ffmpeg/libavutil/frame.c` |
| 4 | `/src/ffmpeg/libavutil/avstring.h` |
| 3 | `/src/ffmpeg/libavutil/dict.c` |
| 2 | `/src/ffmpeg/./libavutil/avstring.h` |
| 1 | `/src/ffmpeg/libavutil/bprint.h` |
| 1 | `/src/ffmpeg/./libavutil/atomic_gcc.h` |
| 1 | `/src/ffmpeg/./libavcodec/bytestream.h` |

The chain is the one the bug report implies: the AVBPrint buffer growth
(`bprint.c`, `mem.c`) that produced the truncated `dst`, the SAMI/HTML parsing
that filled it (`samidec.c`, `htmlsubtitles.c`, `avstring.c`), the packet the
fuzzer handed in (`avpacket.c`, `decode.c`, `target_dec_fuzzer.c`), and the
`fread` of the PoC bytes in `standalone_driver.c`.

Note the digest saturates long before the run ends — all 284 source lines were
already present at 151k values. The value count keeps climbing because the same
source lines recur at many distinct trace indices.

## How the criterion was chosen

The report's frame #0 is `__interceptor_strncmp.part.75`, frame #1 is
`ff_htmlmarkup_to_ass /src/ffmpeg/libavcodec/htmlsubtitles.c:174:30`. That gives
`giri-criterion` the function, the file, the line, the **column** and the
intercepted callee. Exactly one instruction on that line is a `call @strncmp`,
and it is at column 30, so it ranks first — ahead of the seven loads, the
`icmp`, and the loop latch `-criterion-loc` would have picked.

The ranking is a preference, not a proof. The gate is that Giri reports
`[GIRI] criterion never executed` when a criterion's basic block is absent from
the trace, and the driver walks to the next candidate on it. Here the first
candidate executed, so the other seven were never tried.

## Noise the run emits and what it means

Both of these are expected and neither is a gate:

* `[GIRI] Function id on stack doesn't match for id N … MAY be due to function
  call from external code` — 18 lines during the traced run. Calls that entered
  instrumented code from a TU that is not in the bitcode.
* `getSourcesForCall failed to find` / `call records missing` during slicing.
  With `-stats` compiled out (see below) these are the visible proxy for
  untraced dependences.

## Statistics are unavailable in this container

`-stats` is a no-op here: the container's LLVM is a Release build with
assertions off, so `NDEBUG` compiles out `STATISTIC`, including
`Number of Dynamic Loads Lost`. Getting the counters would need an
assertions-enabled LLVM.
