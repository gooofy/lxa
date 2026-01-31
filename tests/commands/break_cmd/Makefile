include ../../Makefile.config

TARGET = break_test

CFLAGS = -march=68000 -mcpu=68000 -O2 -Wall -Werror -I$(BASEDIR)/src/include -mcrt=nix20
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
	@echo "Running test: commands/break_cmd..."
	@rm -f actual.out
	@cd "$(dir $(firstword $(MAKEFILE_LIST)))" && \
	timeout 30 ../../test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" || \
	true
	@diff -q expected.out actual.out && \
		echo "$(GREEN)✓ PASS: commands/break_cmd$(RESET)" || \
		echo "$(RED)✗ FAIL: commands/break_cmd$(RESET)"

.PHONY: all clean run-test
