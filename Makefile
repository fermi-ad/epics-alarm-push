.PHONY: all install buildInstall rebuild clean archclean realclean uninstall realuninstall distclean cvsclean help

# make hiredis static only so epics linking has no choice
all:
	$(MAKE) -C hiredis static
	$(MAKE) -f Makefile.Epics $(MAKECMDGOALS)

# dont allow hiredis to install
install buildInstall rebuild: all

# hiredis doesnt know epics clean targets
clean:
	$(MAKE) -C hiredis clean
	$(MAKE) -f Makefile.Epics $(MAKECMDGOALS)

archclean realclean uninstall realuninstall distclean cvsclean: clean

help:
	$(MAKE) -f Makefile.Epics $(MAKECMDGOALS)
