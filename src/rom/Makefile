include ../../config.mk

#CFLAGS	= -march=68000 -mcpu=68000 -Werror -O2 -fstrength-reduce -fomit-frame-pointer -msmall-code -fbaserel -c
CFLAGS	= -march=68000 -mcpu=68000 -Werror -O2 -fstrength-reduce -fomit-frame-pointer
ASFLAGS = -march=68000 -mcpu=68000 -I/home/guenter/projects/amiga/amiga-gcc/m68k-amigaos/ndk-include
LDFLAGS	= -noixemul -nostartfiles -nostdlib -fbaserel -s
LDLIBS	=

.PHONY: clean all diss

all:	lxa.rom

clean:
		rm -f *.library *.o *.asm t.log *.map *.hunk *.rom

diss:	lxa.rom.hunk
	$(OBJDUMP_AMIGAOS) -s -d -x lxa.rom.hunk

lxa.rom.hunk: romhdr.o exec.o exceptions.o util.o amath.o lxa_dos.o lxa_utility.o lxa_mathffp.o lxa_mathtrans.o lxa_dev_input.o lxa_graphics.o lxa_intuition.o lxa_expansion.o lxa_dev_console.o
	# $(CC_AMIGAOS) $(LDFLAGS) -Xlinker -Map=lxa.rom.map -o $@ $^ $(LDLIBS)
	${LD_AMIGAOS} -T rom.ld -o $@ -Map=lxa.rom.map $^

#exec.library: romhdr.o exec.o exceptions.o util.o amath.o lxa_dos.o lxa_utility.o lxa_mathffp.o lxa_mathtrans.o lxa_dev_input.o lxa_graphics.o lxa_intuition.o lxa_expansion.o lxa_dev_console.o
#exec.library: exec.o exceptions.o util.o amath.o lxa_dos.o lxa_utility.o lxa_mathffp.o lxa_mathtrans.o lxa_dev_input.o lxa_graphics.o lxa_intuition.o lxa_expansion.o lxa_dev_console.o restorea4.o
#	$(CC_AMIGAOS) $(LDFLAGS) -Xlinker -Map=exec.map -o $@ $^ $(LDLIBS)

# mathffp.library: lxa_mathffp.o util.o amath.o
# 	$(CC_AMIGAOS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

lxa.rom.bin: lxa.rom.hunk
	$(OBJCOPY_AMIGAOS) -O binary --only-section=.text lxa.rom.hunk lxa.rom.bin

%.o : %.c
	$(CC_AMIGAOS) -c $(CFLAGS) $< -o $@

%.o : %.s
	$(AS_AMIGAOS) $(ASFLAGS) $< -o $@

%.asm : %.c
	$(CC_AMIGAOS) -S $(CFLAGS) $< -o $@

lxa.rom: lxa.rom.bin
	$(ROMTOOL) build -o lxa.rom -t kick -s 256 lxa.rom.bin
