TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += AlarmPush

# install AlarmPush.dbd into <top>/dbd
DBD += AlarmPush.dbd

# note this needs to have the -I
AlarmPush_INCLUDES += -I../../../hiredisSup/hiredis

AlarmPush_SRCS += AlarmPush.cpp

AlarmPush_LIBS += $(EPICS_BASE_IOC_LIBS)
AlarmPush_LIBS += hiredis

USR_CPPFLAGS += -DUSE_TYPED_RSET

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
