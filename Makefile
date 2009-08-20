all: vomid

Makefile.qmake: libvomid/build/config.mk
	qmake

libvomid/build/libvomid.a: libvomid/build/config.mk
	@cd libvomid && $(MAKE) $(MKOPTS) && cd ..

libvomid/build/config.mk:
	@cd libvomid && ./configure && cd ..

vomid: libvomid/build/libvomid.a libvomid/include/vomid.h Makefile.qmake
	@$(MAKE) $(MKOPTS) -f Makefile.qmake

clean:
	@cd libvomid && make clean && cd ..
	rm -rf build Makefile.qmake vomid vomid.exe

.PHONY: vomid all
