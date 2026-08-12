##===- giri/test/Makefile ----------------------------------*- Makefile -*-===##
#
# Define the default test rule. Just type 'make test' in this directory.
#
##===----------------------------------------------------------------------===##

.PHONY: test lib clean

TEST_LOG ?= /dev/null

# The default target in this directory is the test:: target
test:: lib
	@ echo "Running the test cases..."
	@ mkdir -p _test_logs;\
	  RET=0;\
	  for t in $$(cat auto-tests.txt);\
	  do\
	      _testlog=_test_logs/`echo $$t | tr '/' '_'`.log;\
	      printf "\033[37m%-20s" "$$t";\
		  make clean -s -C $$t > /dev/null 2>&1;\
		  echo "=== STAGE: BUILD ===" > "$$_testlog";\
		  make -s -C $$t DEBUGFLAGS= >> "$$_testlog" 2>&1;\
		  build_status=$$?;\
		  if [ $$build_status -ne 0 ]; then\
			  printf "\033[31m[FAIL build]\033[0m\n";\
		      RET=1;\
			  continue;\
		  fi;\
		  echo "=== STAGE: TEST ===" >> "$$_testlog";\
		  make test -s -C $$t DEBUGFLAGS= >> "$$_testlog" 2>&1;\
		  test_status=$$?;\
		  if [ $$test_status -eq 0 ]; then\
			  printf "\033[34m[PASS]\033[0m\n";\
			  make clean -s -C $$t > /dev/null 2>&1;\
		  else\
		      printf "\033[31m[FAIL test]\033[0m\n";\
		      RET=1;\
		  fi;\
	  done;\
	  if [ "$(TEST_LOG)" != "/dev/null" ]; then \
		  cat _test_logs/*.log > $(TEST_LOG); \
	  fi;\
	  exit $$RET

lib:
	@ echo -n "Building the Giri..."
	@ mkdir -p _test_logs; \
		$(MAKE) -s -C ../build > _test_logs/lib-stage.log 2>&1 || (cat _test_logs/lib-stage.log && echo "Fail to build the Giri lib!" && exit 1)
	@ echo

clean:
	@ for t in $$(cat auto-tests.txt); do make clean -C $$t; done
	@ rm -rf _test_logs
	@ rm -f $(TEST_LOG)