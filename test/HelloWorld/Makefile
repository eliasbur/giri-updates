##===- giri/test/UnitTests/Makefile.common -----------------*- Makefile -*-===##
#

DEBUGFLAGS ?=
CFLAGS ?= -g -O0 -c -emit-llvm
LDFLAGS ?=
BuildMode ?= Release+Debug+Asserts
GIRI_DIR ?= ../../build/
NAME ?= hello-world

CC = clang
CXX = clang++
GIRI_LIB_DIR = $(GIRI_DIR)/lib
GIRI_BIN_DIR = $(GIRI_DIR)/bin

# New pass manager harness (LLVM 15.0.0): each Giri library is loaded twice —
# -load registers the cl::opt globals (-trace-file, -slice-file, ...) before
# opt parses the command line, -load-pass-plugin registers the -passes
# pipeline names after. mergereturn (a function pass) is wrapped in a
# function(...) sub-pipeline; the Giri passes are module passes at top level.
GIRI_LOAD = -load $(GIRI_LIB_DIR)/libdgutility.so -load $(GIRI_LIB_DIR)/libgiri.so
GIRI_LOAD_PLUGINS = -load-pass-plugin=$(GIRI_LIB_DIR)/libdgutility.so -load-pass-plugin=$(GIRI_LIB_DIR)/libgiri.so

.PHONY: all lib

all: lib $(NAME).slice
	@ echo -n "The line numbers of source code slice are: "
	@ sed -n '/^Source.*[0-9]$$/p' $(NAME).slice |\
		awk -F: '{print $$3}' |\
		sort -g |\
		uniq | xargs\
		echo

lib:
	$(MAKE) -s -C $(GIRI_DIR)

$(NAME).slice : $(NAME).bc $(NAME).trace
	@ opt $(GIRI_LOAD) $(GIRI_LOAD_PLUGINS) \
		-passes="function(mergereturn),bbnum,lsnum,dgiri,remove-bbnum,remove-lsnum" \
		-trace-file=$(NAME).trace -slice-file=$(NAME).slice \
		-stats $(DEBUGFLAGS) $< -o /dev/null

$(NAME).trace: $(NAME).trace.exe
	- ./$< $(INPUT)

$(NAME).trace.exe : $(NAME).trace.s
	$(CXX) -fno-strict-aliasing $+ -o $@ $(LDFLAGS) -L$(GIRI_LIB_DIR) -lrtgiri

$(NAME).trace.s : $(NAME).trace.bc
	llc -asm-verbose=false -O0 $< -o $@

$(NAME).trace.bc : $(NAME).bc
	@ opt $(GIRI_LOAD) $(GIRI_LOAD_PLUGINS) \
		-passes="function(mergereturn),bbnum,lsnum,trace-giri,remove-bbnum,remove-lsnum" \
		-trace-file=$(NAME).trace \
		-stats $(DEBUGFLAGS) $< -o $@

%.bc : %.c
	$(CC) $(CFLAGS) $+ -o $@
%.ll : %.bc
	llvm-dis $< -o $@

.PHONY: ptrace rebuild clean

prtrace: $(NAME).trace
	$(GIRI_BIN_DIR)/prtrace $<

rebuild: clean all

clean:
	@ rm -f *.ll *.bc *.o *.s *.slice *.exe *.trace
