include ../../Makefile.config

TARGET := Sift
SAMPLES_DIR := $(BASEDIR)/build/target/samples/Samples

# We need test.iff to be in the same directory as the binary
# The test runner maps TEST_DIR as SYS:
# So we copy test.iff to where the binary is, then remove it after

.PHONY: all clean run-test

all:

clean:
	rm -f actual.out

# Sift needs argument: the IFF file to parse
# Copy test.iff to samples dir temporarily, run test, then clean up
run-test:
	@cp test.iff "$(SAMPLES_DIR)/test.iff"
	@../../test_runner_with_args.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(SAMPLES_DIR)/$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" "test.iff" || (rm -f "$(SAMPLES_DIR)/test.iff"; exit 1)
	@rm -f "$(SAMPLES_DIR)/test.iff"
