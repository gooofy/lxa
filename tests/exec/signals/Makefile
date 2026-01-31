include ../../Makefile.config

TARGET = signals

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
	@echo "Running test: exec/signals..."
	@rm -f actual.out
	@./run_test.sh && \
		echo "$(GREEN)✓ PASS: exec/signals$(RESET)" || \
		echo "$(RED)✗ FAIL: exec/signals$(RESET)"

.PHONY: all clean run-test
