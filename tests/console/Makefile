# Console escape sequence tests

SUBDIRS = csi_cursor input_inject input_console kp2_test

.PHONY: all clean run-tests $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done

run-tests:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir run-test; done
