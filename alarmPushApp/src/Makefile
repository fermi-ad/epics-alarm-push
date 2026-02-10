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

# note this needs to have the -I
alarmPush_INCLUDES += -I$(TOP)/hiredis

alarmPush_SRCS += alarmPush.cpp

alarmPush_LIBS += $(EPICS_BASE_IOC_LIBS)
alarmPush_LIBS += hiredis

hiredis_DIR = $(TOP)/hiredis

USR_CPPFLAGS += -DUSE_TYPED_RSET

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
