include ../../config.mk

SRCS = 	lxa.c m68kcpu.c  m68kdasm.c  m68kops.c softfloat.c util.c vfs.c config.c

OBJS_LINUX   := $(patsubst %,${OBJDIR_LINUX}/%.o,$(basename $(SRCS)))

.PHONY: clean all
all:	$(BINDIR_LINUX)/lxa

clean:
	rm -f $(OBJDIR_LINUX)/*.o $(BINDIR_LINUX)/lxa
	rm -rf $(DEPDIR_LINUX)

$(OBJDIR_LINUX)/%.o : %.c $(DEPDIR_LINUX)/%.d | $(DEPDIR_LINUX)
	$(CC_LINUX) -c $(CFLAGS_LINUX) $(DEPFLAGS_LINUX) $< -o $@

${BINDIR_LINUX}/lxa:	$(OBJS_LINUX)
	$(CC_LINUX) $(CFLAGS_LINUX) $(LDFLAGS_LINUX) -pthread -lreadline -o $@ $^

$(DEPDIR_LINUX):
	@mkdir -p $@

DEPFILES_LINUX := $(SRCS:%.c=$(DEPDIR_LINUX)/%.d)
$(DEPFILES_LINUX):

include $(wildcard $(DEPFILES_LINUX))

