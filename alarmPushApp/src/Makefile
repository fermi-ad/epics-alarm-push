TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += alarmPush

# install alarmPush.dbd into <top>/dbd
DBD += alarmPush.dbd

# specify all source files to be compiled and added to the library
alarmPush_SRCS += alarmPush.cpp

alarmPush_LIBS += $(EPICS_BASE_IOC_LIBS)

alarmPush_LIBS += hiredis
hiredis_DIR = $(TOP)/hiredis

CPPFLAGS += -DUSE_TYPED_RSET

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
