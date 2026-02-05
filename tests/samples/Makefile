include ../Makefile.config

TESTS := SimpleTask Signals Semaphore BuildList Allocate AllocEntry DeviceUse TimerSoftInt TaskList Tag1 OpenWindowTags Hooks1 SimpleGad UpdateStrGad IntuiText SimpleImage ShadowBorder EasyIntuition SimpleMenu EasyRequest SimpleTimer Clipping SimpleGTGadget FFPExample Sift FindBoards

# NOTE: The following samples are NOT included in automated tests:
# - Port1/Port2: Require concurrent execution (both programs must run 
#   simultaneously to communicate via message ports).
# These samples are kept in the samples/ directory for manual testing but are
# not suitable for the automated test suite.

.PHONY: all clean run-test $(TESTS)

all: $(TESTS)

$(TESTS):
	@$(MAKE) -C $@

clean:
	@for dir in $(TESTS); do $(MAKE) -C $$dir clean; done

run-test:
	@for dir in $(TESTS); do $(MAKE) -C $$dir run-test || exit 1; done
