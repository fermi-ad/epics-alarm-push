.PHONY: all install clean distclean realclean

# make hiredis static only so epics linking has no choice
all:
	$(MAKE) -C hiredis static
	$(MAKE) -f Makefile.Epics

# dont allow hiredis to install
install: all

# hiredis doesnt know epics fancy clean targets
clean:
	$(MAKE) -C hiredis clean
	$(MAKE) -f Makefile.Epics $(MAKECMDGOALS)

distclean: clean
realclean: clean
