##===- giri/test/UnitTests/test12/Makefile -----------------*- Makefile -*-===##

NAME = psum
LDFLAGS = -pthread
INPUT ?= 16 8
# pthread_join reuses single tid — undefined; actual exit 28 on LLVM 5.0.2
EXPECTED_EXIT = 28

include ../../Makefile.common