##===- giri/test/UnitTests/test11/Makefile -----------------*- Makefile -*-===##

NAME = hello2p
LDFLAGS = -pthread
INPUT ?= 4
# pthread_join reuses single tid — undefined; actual exit 52 on LLVM 5.0.2
EXPECTED_EXIT = 52

include ../../Makefile.common