# Slice result — ffmpeg SAMI fuzzer (ARVO 42473917)

Container `arvo-42473917-vul`, bug: heap-buffer-overflow read at
`libavcodec/htmlsubtitles.c:174` (ASan frame #1).

    criterion : ff_htmlmarkup_to_ass:571   (%503 = call i32 @strncmp(...))
    trace     : 28 MB, 899,276 entries, from ./…trace.exe /tmp/poc
    module    : 91 TUs, 5.0 MB .all.bc
    command   : giri-slice <base> ff_htmlmarkup_to_ass:571   (with -slice-terse)

**Complete run** (exit 0), with patch 0004 applied:

| | |
|---|---|
| wall time | **23 s** |
| dynamic values in slice | 608,164 |
| distinct source lines | 284 |
| slice file | 57 MB (terse) |

Without patch 0004 the same slice never finished: 50 minutes produced 256,936
of the 608,164 values (~42 %) and was still going. The source-line digest is
byte-identical in both modes — verified against this run and against the
`test2` golden.

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

## Statistics are unavailable in this container

`-stats` is a no-op here: the container's LLVM is a Release build with
assertions off, so `NDEBUG` compiles out `STATISTIC`, including
`Number of Dynamic Loads Lost`. The visible proxy for untraced dependences is
the `getSourcesForCall failed to find` / `call records missing` lines on stderr.
Getting the counters would need an assertions-enabled LLVM.
