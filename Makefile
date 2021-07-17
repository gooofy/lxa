.PHONY: clean all src

all: directories src

src:
	cd src ; make all

directories:
	mkdir -p target/x86_64-linux/bin
	mkdir -p target/x86_64-linux/obj

clean:
	cd src ; make clean
	rm -rf target

