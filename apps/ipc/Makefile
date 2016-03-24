#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# Targets
TARGETS := $(notdir $(SOURCE_DIR)).bin

# Make sure this symbol stays around as we don't reference this, but
# whoever loads us will
LDFLAGS += -u __vsyscall_ptr

# Source files required to build the target 
CFILES :=  $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))

# Libraries
LIBS := sel4 c cpio sel4muslcsys utils sel4utils sel4allocman elf sel4vspace sel4simple sel4vka\
	    sel4bench sel4benchsupport  

HDRFILES := $(wildcard ${SOURCE_DIR}/include/arch/${ARCH}/arch/*)
ifeq (${ARCH},arm)
	HDRFILES += $(wildcard ${SOURCE_DIR}/include/arch/${ARCH}/armv/${ARMV}/*)
endif


include $(SEL4_COMMON)/common.mk
