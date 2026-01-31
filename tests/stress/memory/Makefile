include ../../Makefile.config

TARGET = memory_stress

CFLAGS = -march=68000 -mcpu=68000 -O2 -Wall -I$(BASEDIR)/src/include -mcrt=nix20
LDFLAGS = -march=68000 -mcpu=68000 -mcrt=nix20

OBJS = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC_AMIGAOS) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC_AMIGAOS) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) actual.out lxa.log

run-test: $(TARGET)
	@echo "Running test: stress/memory..."
	@rm -f actual.out lxa.log
	@timeout 120 ./run_test.sh && \
		echo "$(GREEN)PASS: stress/memory$(RESET)" || \
		echo "$(RED)FAIL: stress/memory$(RESET)"

.PHONY: all clean run-test
