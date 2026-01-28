# Integration Test Suite

include Makefile.config

# Test categories
TEST_CATS := dos exec

# Color output
GREEN = \033[0;32m
RED = \033[0;31m
RESET = \033[0m

.PHONY: all clean test run-tests help

all:
	@echo "Building all tests..."
	@$(MAKE) -C dos/helloworld
	@$(MAKE) -C dos/lock_examine
	@$(MAKE) -C exec/signal_pingpong

clean:
	@echo "Cleaning all tests..."
	@$(MAKE) -C dos/helloworld clean
	@$(MAKE) -C dos/lock_examine clean
	@$(MAKE) -C exec/signal_pingpong clean
	rm -f dos/helloworld/actual.out
	rm -f dos/lock_examine/actual.out
	rm -f exec/signal_pingpong/actual.out

test: all
	@echo ""
	@echo "Running integration tests..."
	@echo ""
	$(MAKE) run-tests

run-tests:
	@$(MAKE) -C dos/helloworld run-test
	@$(MAKE) -C dos/lock_examine run-test
	@$(MAKE) -C exec/signal_pingpong run-test

help:
	@echo "Integration Test Suite"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build all test binaries"
	@echo "  test        - Build and run all tests"
	@echo "  clean       - Clean all test artifacts"
	@echo "  help        - Show this help"
