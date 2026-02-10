.PHONY: all install clean distclean realclean

all:
	$(MAKE) -C hiredis
	rm hiredis/libhiredis.so
	$(MAKE) -f Makefile.Epics

install: all

clean:
	$(MAKE) -C hiredis clean
	$(MAKE) -f Makefile.Epics $(MAKECMDGOALS)

distclean: clean

realclean: clean
