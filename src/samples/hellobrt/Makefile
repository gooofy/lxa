AQBDIR       = ../../../../tiger/aqb
AQB			 = $(AQBDIR)/target/x86_64-linux/bin/aqb
AQBFLAGS     = -L $(AQBDIR)/src/lib -d _brt

.PHONY: clean all

all: hello

hello: hello.bas
	$(AQB) $(AQBFLAGS) -o $@ $<
	cp $@ $(XAMIGA)

clean:
	rm -f *.o hello

