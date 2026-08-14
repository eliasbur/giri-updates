##===- giri/test/UnitTests/test2/Makefile ------------------*- Makefile -*-===##

NAME = ifelse
INPUT ?= a b c d
# argc=5, func(8)=11, 5<11 → x=argc-3=2
# Note: 2 == SIGINT; if the program crashes with SIGINT, the trace stage will
# fail on the [GIRI] Abnormal termination marker before this exit-code check.
EXPECTED_EXIT = 2

include ../../Makefile.common