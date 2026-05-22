TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += AlarmPush

DBD += AlarmPush.dbd

# note this needs to have the -I
AlarmPush_INCLUDES += -I../hiredis

AlarmPush_SRCS += AlarmPush.cpp

SRC_DIRS += ../hiredis

AlarmPush_SRCS += alloc.c
AlarmPush_SRCS += net.c
AlarmPush_SRCS += hiredis.c
AlarmPush_SRCS += sds.c
AlarmPush_SRCS += async.c
AlarmPush_SRCS += read.c
AlarmPush_SRCS += sockcompat.c

AlarmPush_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_CPPFLAGS += -DUSE_TYPED_RSET

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
