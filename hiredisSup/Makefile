TOP=..

include $(TOP)/configure/CONFIG

SHARED_LIBRARIES = NO

LIBRARY_IOC += hiredis

SRC_DIRS += ../hiredis

hiredis_SRCS += alloc.c
hiredis_SRCS += net.c
hiredis_SRCS += hiredis.c
hiredis_SRCS += sds.c
hiredis_SRCS += async.c
hiredis_SRCS += read.c
hiredis_SRCS += sockcompat.c

include $(TOP)/configure/RULES
