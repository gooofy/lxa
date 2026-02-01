include ../../Makefile.config

TARGET = matchfirst

CFLAGS = -march=68000 -mcpu=68000 -O2 -Wall -I$(BASEDIR)/src/include -mcrt=nix20
LDFLAGS = -march=68000 -mcpu=68000 -mcrt=nix20

OBJS = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC_AMIGAOS) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC_AMIGAOS) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) actual.out test1.txt test2.txt test3.dat

run-test: $(TARGET)
	@echo "Running test: dos/matchfirst..."
	@rm -f actual.out test1.txt test2.txt test3.dat
	@cd "$(dir $(firstword $(MAKEFILE_LIST)))" && \
	timeout 30 ../../test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" || true
	@diff -q expected.out actual.out && \
		echo "$(GREEN)✓ PASS: dos/matchfirst$(RESET)" || \
		echo "$(RED)✗ FAIL: dos/matchfirst$(RESET)"

.PHONY: all clean run-test
