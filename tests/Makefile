# Integration Test Suite

include Makefile.config

# Test categories
TEST_CATS := dos exec shell

# Color output
GREEN = \033[0;32m
RED = \033[0;31m
RESET = \033[0m

.PHONY: all clean test run-tests help

all:
	@echo "Building all tests..."
	@$(MAKE) -C dos/helloworld
	@$(MAKE) -C dos/lock_examine
	@$(MAKE) -C dos/lock_shared
	@$(MAKE) -C dos/fileio
	@$(MAKE) -C dos/readargs
	@$(MAKE) -C dos/patterns
	@$(MAKE) -C dos/dir_command
	@$(MAKE) -C exec/signal_pingpong
	@$(MAKE) -C shell/alias
	@$(MAKE) -C shell/controlflow
	@$(MAKE) -C shell/script

clean:
	@echo "Cleaning all tests..."
	@$(MAKE) -C dos/helloworld clean
	@$(MAKE) -C dos/lock_examine clean
	@$(MAKE) -C dos/lock_shared clean
	@$(MAKE) -C dos/fileio clean
	@$(MAKE) -C dos/readargs clean
	@$(MAKE) -C dos/patterns clean
	@$(MAKE) -C dos/dir_command clean
	@$(MAKE) -C exec/signal_pingpong clean
	@$(MAKE) -C shell/alias clean
	@$(MAKE) -C shell/controlflow clean
	@$(MAKE) -C shell/script clean
	rm -f dos/helloworld/actual.out
	rm -f dos/lock_examine/actual.out
	rm -f dos/lock_shared/actual.out
	rm -f dos/fileio/actual.out
	rm -f dos/readargs/actual.out
	rm -f dos/patterns/actual.out
	rm -f dos/dir_command/actual.out
	rm -f exec/signal_pingpong/actual.out
	rm -f shell/alias/actual.out
	rm -f shell/controlflow/actual.out
	rm -f shell/script/actual.out

test: all
	@echo ""
	@echo "Running integration tests..."
	@echo ""
	$(MAKE) run-tests

run-tests:
	@$(MAKE) -C dos/helloworld run-test
	@$(MAKE) -C dos/lock_examine run-test
	@$(MAKE) -C dos/lock_shared run-test
	@$(MAKE) -C dos/fileio run-test
	@$(MAKE) -C dos/readargs run-test
	@$(MAKE) -C dos/patterns run-test
	@$(MAKE) -C dos/dir_command run-test
	@$(MAKE) -C exec/signal_pingpong run-test
	@$(MAKE) -C shell/alias run-test
	@$(MAKE) -C shell/controlflow run-test
	@$(MAKE) -C shell/script run-test

help:
	@echo "Integration Test Suite"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build all test binaries"
	@echo "  test        - Build and run all tests"
	@echo "  clean       - Clean all test artifacts"
	@echo "  help        - Show this help"
