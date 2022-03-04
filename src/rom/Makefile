include ../../config.mk

CFLAGS	= -march=68000 -mcpu=68000 -Werror -O2 -fstrength-reduce -fomit-frame-pointer -msmall-code -fbaserel -c
ASFLAGS = -march=68000 -mcpu=68000 -I/home/guenter/projects/amiga/amiga-gcc/m68k-amigaos/ndk-include
LDFLAGS	= -noixemul -nostartfiles -nostdlib -fbaserel -s
LDLIBS	=

.PHONY: clean all diss

all:	exec.library mathffp.library my.rom

clean:
		rm -f *.library *.o my.rom *.asm t.log

diss:
	$(OBJDUMP_AMIGAOS) -S -d -x exec.library

exec.library: romhdr.o exec.o exceptions.o util.o amath.o lxa_dos.o lxa_utility.o
	$(CC_AMIGAOS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

mathffp.library: lxa_mathffp.o util.o amath.o
	$(CC_AMIGAOS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o : %.c
	$(CC_AMIGAOS) -c $(CFLAGS) $< -o $@

%.o : %.s
	$(AS_AMIGAOS) $(ASFLAGS) $< -o $@

%.asm : %.c
	$(CC_AMIGAOS) -S $(CFLAGS) $< -o $@

my.rom:	exec.library mathffp.library
	$(ROMTOOL) build -o my.rom -t kick -s 256 exec.library mathffp.library
