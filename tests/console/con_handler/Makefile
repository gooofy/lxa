include ../../Makefile.config

TARGET = con_handler

CFLAGS = -march=68000 -mcpu=68000 -O2 -Wall -Werror -I$(BASEDIR)/src/include -I$(BASEDIR)/tests/common -mcrt=nix20
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
	@echo "Running test: console/con_handler..."
	@rm -f actual.out
	@cd "$(dir $(firstword $(MAKEFILE_LIST)))" && \
	timeout 30 ../../test_runner.sh "$(LXA_BIN)" "$(LXA_ROM)" "$(TARGET)" "expected.out" "actual.out" "$(LXA_SYS)" || true
	@diff -q expected.out actual.out && \
		echo "$(GREEN)PASS: console/con_handler$(RESET)" || \
		(echo "$(RED)FAIL: console/con_handler$(RESET)" && diff expected.out actual.out)

.PHONY: all clean run-test
