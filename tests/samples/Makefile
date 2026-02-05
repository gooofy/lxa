include ../Makefile.config

TESTS := SimpleTask Signals Semaphore BuildList Allocate AllocEntry DeviceUse TimerSoftInt TaskList Tag1 IStr A2D OpenWindowTags Hooks1 UpdateStrGad IntuiText SimpleImage ShadowBorder EasyIntuition SimpleMenu EasyRequest SimpleTimer Clipping SimpleGTGadget FFPExample FFPTrans Sift FindBoards CustomPointer CloneScreen PublicScreen VisibleWindow WinPubScreen MeasureText AvailFonts GadToolsMenu Talk2Boopsi FileReq FontReq GadToolsGadgets Uptime WBClone AskKeymap

# NOTE: The following samples are NOT included in automated tests:
# - Port1/Port2: Require concurrent execution (both programs must run 
#   simultaneously to communicate via message ports).
# - SimpleGad: Interactive sample that waits for user input. Use the host-side
#   test driver (tests/drivers/simplegad_test) for automated testing.
# - MouseTest/RawKey: Interactive samples that wait for user input. These need
#   host-side test drivers for automated testing.
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
