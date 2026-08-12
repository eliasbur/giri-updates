##===- giri/test/UnitTests/test21/Makefile -----------------*- Makefile -*-===##

NAME = hwtype
INPUT ?= t Giri
CRITERION ?= -criterion-inst=criterion-inst.txt
# NULL pointer dereference in printf → SIGSEGV → exit 128+11=139
EXPECTED_EXIT = 139

include ../../Makefile.common