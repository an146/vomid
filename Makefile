all: vomid

Makefile.qmake: libvomid/build/config.mk
	qmake

libvomid/build/config.mk libvomid/build/libvomid.a: .FORCE
	@cd libvomid && $(MAKE) $(MKOPTS) && cd ..

vomid: libvomid/build/libvomid.a libvomid/include/vomid.h Makefile.qmake
	@$(MAKE) $(MKOPTS) -f Makefile.qmake

clean:
	@cd libvomid && make clean && cd ..
	rm -rf build Makefile.qmake vomid vomid.exe

.FORCE:

.PHONY: vomid all .FORCE
