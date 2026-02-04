include ../Makefile.config

TESTS := SimpleTask Signals Semaphore BuildList Allocate DeviceUse Tag1 OpenWindowTags Hooks1 Port1 Port2

.PHONY: all clean run-test $(TESTS)

all: $(TESTS)

$(TESTS):
	@$(MAKE) -C $@

clean:
	@for dir in $(TESTS); do $(MAKE) -C $$dir clean; done

run-test:
	@for dir in $(TESTS); do $(MAKE) -C $$dir run-test || exit 1; done
