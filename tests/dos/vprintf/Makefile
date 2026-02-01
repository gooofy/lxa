include ../../Makefile.config

TARGET = vprintf

CFLAGS = -march=68000 -mcpu=68000 -O2 -Wall -Werror -I$(BASEDIR)/src/include -mcrt=nix20
LDFLAGS = -march=68000 -mcpu=68000 -mcrt=nix20

OBJS = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC_AMIGAOS) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC_AMIGAOS) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) actual.out testfile.txt

run-test: $(TARGET)
	@echo "Running test: dos/vprintf..."
	@rm -f actual.out testfile.txt
	@cd "$(dir $(firstword $(MAKEFILE_LIST)))" && \
	timeout 30 ../../test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" || true
	@diff -q expected.out actual.out && \
		echo "$(GREEN)✓ PASS: dos/vprintf$(RESET)" || \
		echo "$(RED)✗ FAIL: dos/vprintf$(RESET)"

.PHONY: all clean run-test
