.PHONY: all install buildInstall rebuild clean distclean realclean archclean help

# make hiredis static only so epics linking has no choice
all:
	$(MAKE) -C hiredis static
	$(MAKE) -f Makefile.Epics $(MAKECMDGOALS)

help:
	$(MAKE) -f Makefile.Epics help

# dont allow hiredis to install
install buildInstall rebuild: all

# hiredis doesnt know epics fancy clean targets
clean:
	$(MAKE) -C hiredis clean
	$(MAKE) -f Makefile.Epics $(MAKECMDGOALS)

archclean realclean uninstall realuninstall distclean cvsclean: clean
