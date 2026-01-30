include ../../Makefile.config

TARGET = test_script
SCRIPT = script.txt

CFLAGS = -march=68000 -mcpu=68000 -O2 -Wall -I$(BASEDIR)/src/include -mcrt=nix20
LDFLAGS = -march=68000 -mcpu=68000 -mcrt=nix20

OBJS = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC_AMIGAOS) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC_AMIGAOS) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) actual.out

run-test: $(TARGET)
	@echo "Running test: shell/script..."
	@rm -f actual.out
	@cd "$(dir $(firstword $(MAKEFILE_LIST)))" && \
	../../test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" || \
	../../test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" || \
	true
	@diff -q expected.out actual.out && \
		echo "$(GREEN)✓ PASS: shell/script$(RESET)" || \
		echo "$(RED)✗ FAIL: shell/script$(RESET)"

.PHONY: all clean run-test
