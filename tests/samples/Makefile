include ../Makefile.config

TESTS := SimpleTask Signals Semaphore Port1 Port2 BuildList Allocate DeviceUse TimerSoftInt Tag1 OpenWindowTags

.PHONY: all clean run-test $(TESTS)

all: $(TESTS)

$(TESTS):
	@$(MAKE) -C $@

clean:
	@for dir in $(TESTS); do $(MAKE) -C $$dir clean; done

run-test:
	@for dir in $(TESTS); do $(MAKE) -C $$dir run-test || exit 1; done
