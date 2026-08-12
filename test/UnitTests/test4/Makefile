##===- giri/test/UnitTests/test4/Makefile ------------------*- Makefile -*-===##

NAME = example
LDFLAGS = -lm
INPUT ?= 10
# sqrt(10*3) = sqrt(30) ≈ 5.477 → cast to int = 5
EXPECTED_EXIT = 5
CRITERION ?= -criterion-loc=criterion-loc.txt
#CRITERION ?= -criterion-inst=criterion-inst.txt
MAPPING ?= -mapping-function=main

include ../../Makefile.common