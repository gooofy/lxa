include ../../Makefile.config

TARGET = execute_script

CFLAGS = -march=68000 -mcpu=68000 -O2 -Wall -Werror -I$(BASEDIR)/src/include -mcrt=nix20
LDFLAGS = -march=68000 -mcpu=68000 -mcrt=nix20

OBJS = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC_AMIGAOS) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC_AMIGAOS) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) actual.out lxa.log execute_test_out.txt

run-test: $(TARGET)
	@echo "Running test: dos/execute_script..."
	@rm -f actual.out
	@cd "$(dir $(firstword $(MAKEFILE_LIST)))" && \
	timeout 30 ../../test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" || \
	true
	@diff -q expected.out actual.out && \
		echo "$(GREEN)✓ PASS: dos/execute_script$(RESET)" || \
		echo "$(RED)✗ FAIL: dos/execute_script$(RESET)"

.PHONY: all clean run-test
