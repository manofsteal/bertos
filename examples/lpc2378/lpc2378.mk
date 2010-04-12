#
# Copyright 2009 Develer S.r.l. (http://www.develer.com/)
# All rights reserved.
#
# Makefile template for BeRTOS wizard.
#
# Author: Lorenzo Berni <duplo@develer.com>
#
#

# Programmer interface configuration, see http://dev.bertos.org/wiki/ProgrammerInterface for help
lpc2378_PROGRAMMER_TYPE = jtag-tiny
lpc2378_PROGRAMMER_PORT = none

# Files included by the user.
lpc2378_USER_CSRC = \
	examples/lpc2378/main.c \
	bertos/drv/timer.c \
	bertos/drv/timer_test.c \
	bertos/cpu/arm/drv/vic_lpc2.c \
	bertos/cpu/arm/drv/timer_lpc2.c \
	bertos/mware/event.c \
	bertos/kern/proc.c \
	bertos/kern/coop.c \
	bertos/kern/preempt.c \
	bertos/kern/proc_test.c \
	bertos/kern/monitor.c \
	bertos/mware/sprintf.c \
	bertos/struct/heap.c \
	#

# Files included by the user.
lpc2378_USER_PCSRC = \
	#

# Files included by the user.
lpc2378_USER_CPPASRC = \
	bertos/cpu/arm/hw/switch_ctx_arm.S \
	#

# Files included by the user.
lpc2378_USER_CXXSRC = \
	#

# Files included by the user.
lpc2378_USER_ASRC = \
	#

# Flags included by the user.
lpc2378_USER_LDFLAGS = \
	#

# Flags included by the user.
lpc2378_USER_CPPAFLAGS = \
	#

# Flags included by the user.
lpc2378_USER_CPPFLAGS = \
	-fno-strict-aliasing \
	-fwrapv \
	#

# Include the mk file generated by the wizard
include examples/lpc2378/lpc2378_wiz.mk
