include ../../Makefile.config

TARGET := WinPubScreen
SAMPLES_DIR := $(BASEDIR)/build/target/samples/Samples

.PHONY: all clean run-test

all:

clean:
	rm -f actual.out

run-test:
	@../../test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(SAMPLES_DIR)/$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)"
