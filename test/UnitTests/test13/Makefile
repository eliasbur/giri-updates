##===- giri/test/UnitTests/test13/Makefile -----------------*- Makefile -*-===##

NAME = struct
LDFLAGS = -lm
INPUT ?= 10
# printf("%d\n", 5) prints 2 chars
# Note: 2 == SIGINT; if the program crashes with SIGINT, the trace stage will
# fail on the [GIRI] Abnormal termination marker before this exit-code check.
EXPECTED_EXIT = 2

include ../../Makefile.common