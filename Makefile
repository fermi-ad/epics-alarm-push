.PHONY: all clean distclean

all:
	$(MAKE) -C hiredis
	rm hiredis/libhiredis.so
	$(MAKE) -f Makefile.Epics

clean:
	$(MAKE) -C hiredis clean
	$(MAKE) -f Makefile.Epics clean

distclean:
	$(MAKE) -C hiredis clean
	$(MAKE) -f Makefile.Epics distclean
