# Integration Test Suite

include Makefile.config

# Test categories
TEST_CATS := dos exec shell commands

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
	@$(MAKE) -C dos/fileio_advanced
	@$(MAKE) -C dos/readargs
	@$(MAKE) -C dos/readargs_full
	@$(MAKE) -C dos/patterns
	@$(MAKE) -C dos/dir_command
	@$(MAKE) -C dos/createdir
	@$(MAKE) -C dos/delete_rename
	@$(MAKE) -C dos/examine_edge
	@$(MAKE) -C dos/special_chars
	@$(MAKE) -C dos/dir_enum
	@$(MAKE) -C dos/lock_leak
	@$(MAKE) -C dos/fileio_errors
	@$(MAKE) -C dos/seek_console
	@$(MAKE) -C dos/readargs_multi
	@$(MAKE) -C dos/process
	@$(MAKE) -C dos/rename_open
	@$(MAKE) -C dos/readargs_empty
	@$(MAKE) -C dos/execute_script
	@$(MAKE) -C dos/readargs_edge
	@$(MAKE) -C dos/process_advanced
	@$(MAKE) -C dos/createdir_perms
	@$(MAKE) -C exec/signal_pingpong
	@$(MAKE) -C shell/alias
	@$(MAKE) -C shell/controlflow
	@$(MAKE) -C shell/script
	@$(MAKE) -C commands/copy
	@$(MAKE) -C commands/rename
	@$(MAKE) -C commands/join
	@$(MAKE) -C commands/protect
	@$(MAKE) -C commands/filenote
	@$(MAKE) -C commands/list
	@$(MAKE) -C commands/search
	@$(MAKE) -C commands/sort
	@$(MAKE) -C commands/eval
	@$(MAKE) -C commands/setenv
	@$(MAKE) -C commands/syscmd

clean:
	@echo "Cleaning all tests..."
	@$(MAKE) -C dos/helloworld clean
	@$(MAKE) -C dos/lock_examine clean
	@$(MAKE) -C dos/lock_shared clean
	@$(MAKE) -C dos/fileio clean
	@$(MAKE) -C dos/fileio_advanced clean
	@$(MAKE) -C dos/readargs clean
	@$(MAKE) -C dos/readargs_full clean
	@$(MAKE) -C dos/patterns clean
	@$(MAKE) -C dos/dir_command clean
	@$(MAKE) -C dos/createdir clean
	@$(MAKE) -C dos/delete_rename clean
	@$(MAKE) -C dos/examine_edge clean
	@$(MAKE) -C dos/special_chars clean
	@$(MAKE) -C dos/dir_enum clean
	@$(MAKE) -C dos/lock_leak clean
	@$(MAKE) -C dos/fileio_errors clean
	@$(MAKE) -C dos/seek_console clean
	@$(MAKE) -C dos/readargs_multi clean
	@$(MAKE) -C dos/process clean
	@$(MAKE) -C dos/rename_open clean
	@$(MAKE) -C dos/readargs_empty clean
	@$(MAKE) -C dos/execute_script clean
	@$(MAKE) -C dos/readargs_edge clean
	@$(MAKE) -C dos/process_advanced clean
	@$(MAKE) -C dos/createdir_perms clean
	@$(MAKE) -C exec/signal_pingpong clean
	@$(MAKE) -C shell/alias clean
	@$(MAKE) -C shell/controlflow clean
	@$(MAKE) -C shell/script clean
	@$(MAKE) -C commands/copy clean
	@$(MAKE) -C commands/rename clean
	@$(MAKE) -C commands/join clean
	@$(MAKE) -C commands/protect clean
	@$(MAKE) -C commands/filenote clean
	@$(MAKE) -C commands/list clean
	@$(MAKE) -C commands/search clean
	@$(MAKE) -C commands/sort clean
	@$(MAKE) -C commands/eval clean
	@$(MAKE) -C commands/setenv clean
	@$(MAKE) -C commands/syscmd clean
	rm -f dos/helloworld/actual.out
	rm -f dos/lock_examine/actual.out
	rm -f dos/lock_shared/actual.out
	rm -f dos/fileio/actual.out
	rm -f dos/fileio_advanced/actual.out
	rm -f dos/readargs/actual.out
	rm -f dos/readargs_full/actual.out
	rm -f dos/patterns/actual.out
	rm -f dos/dir_command/actual.out
	rm -f dos/createdir/actual.out
	rm -f dos/delete_rename/actual.out
	rm -f dos/examine_edge/actual.out
	rm -f dos/special_chars/actual.out
	rm -f dos/dir_enum/actual.out
	rm -f dos/lock_leak/actual.out
	rm -f dos/fileio_errors/actual.out
	rm -f dos/seek_console/actual.out
	rm -f dos/readargs_multi/actual.out
	rm -f dos/process/actual.out
	rm -f dos/rename_open/actual.out
	rm -f dos/readargs_empty/actual.out
	rm -f dos/execute_script/actual.out
	rm -f dos/readargs_edge/actual.out
	rm -f dos/process_advanced/actual.out
	rm -f dos/createdir_perms/actual.out
	rm -f exec/signal_pingpong/actual.out
	rm -f shell/alias/actual.out
	rm -f shell/controlflow/actual.out
	rm -f shell/script/actual.out
	rm -f commands/copy/actual.out
	rm -f commands/rename/actual.out
	rm -f commands/join/actual.out
	rm -f commands/protect/actual.out
	rm -f commands/filenote/actual.out
	rm -f commands/list/actual.out
	rm -f commands/search/actual.out
	rm -f commands/sort/actual.out
	rm -f commands/eval/actual.out
	rm -f commands/setenv/actual.out
	rm -f commands/syscmd/actual.out

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
	@$(MAKE) -C dos/fileio_advanced run-test
	@$(MAKE) -C dos/readargs run-test
	@$(MAKE) -C dos/readargs_full run-test
	@$(MAKE) -C dos/patterns run-test
	@$(MAKE) -C dos/dir_command run-test
	@$(MAKE) -C dos/createdir run-test
	@$(MAKE) -C dos/delete_rename run-test
	@$(MAKE) -C dos/examine_edge run-test
	@$(MAKE) -C dos/special_chars run-test
	@$(MAKE) -C dos/dir_enum run-test
	@$(MAKE) -C dos/lock_leak run-test
	@$(MAKE) -C dos/fileio_errors run-test
	@$(MAKE) -C dos/seek_console run-test
	@$(MAKE) -C dos/readargs_multi run-test
	@$(MAKE) -C dos/process run-test
	@$(MAKE) -C dos/rename_open run-test
	@$(MAKE) -C dos/readargs_empty run-test
	@$(MAKE) -C dos/execute_script run-test
	@$(MAKE) -C dos/readargs_edge run-test
	@$(MAKE) -C dos/process_advanced run-test
	@$(MAKE) -C dos/createdir_perms run-test
	@$(MAKE) -C exec/signal_pingpong run-test
	@$(MAKE) -C shell/alias run-test
	@$(MAKE) -C shell/controlflow run-test
	@$(MAKE) -C shell/script run-test
	@$(MAKE) -C commands/copy run-test
	@$(MAKE) -C commands/rename run-test
	@$(MAKE) -C commands/join run-test
	@$(MAKE) -C commands/protect run-test
	@$(MAKE) -C commands/filenote run-test
	@$(MAKE) -C commands/list run-test
	@$(MAKE) -C commands/search run-test
	@$(MAKE) -C commands/sort run-test
	@$(MAKE) -C commands/eval run-test
	@$(MAKE) -C commands/setenv run-test
	@$(MAKE) -C commands/syscmd run-test

help:
	@echo "Integration Test Suite"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build all test binaries"
	@echo "  test        - Build and run all tests"
	@echo "  clean       - Clean all test artifacts"
	@echo "  help        - Show this help"
