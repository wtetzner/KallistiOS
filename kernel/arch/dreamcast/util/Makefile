# KallistiOS ##version##
#
# arch/dreamcast/util/Makefile
# Copyright (C) 2001 Dan Potter
#

OBJS = vmu_pkg.o screenshot.o minifont.o
SUBDIRS =

ifneq ($(KOS_SUBARCH), naomi)
	OBJS += fb_console.o
endif

include $(KOS_BASE)/Makefile.prefab
