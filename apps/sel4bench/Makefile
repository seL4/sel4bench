#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(NICTA_GPL)
#

#Targets
TARGETS := sel4bench.bin

# Set custom entry point as we are a rootserver and will not be
# started in a standard way
ENTRY_POINT := _sel4_start

# Source files required to build the target
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/plat/${PLAT}/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/arch/${ARCH}/*.c))

HDRFILES := $(wildcard ${SOURCE_DIR}/include/arch/${ARCH}/arch/*)
ifeq (${ARCH},arm)
HDRFILES += $(wildcard ${SOURCE_DIR}/include/arch/${ARCH}/armv/${ARMV}/*)
endif

ASMFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/crt/arch-${ARCH}/crt0.S))
ASMFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/arch/${ARCH}/*.S))
ASMFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/plat/${PLAT}/*.S))

# We want to run C99
CFLAGS += -std=gnu99

# Optimize for size
CFLAGS += -Os

# Libraries required to build the target
LIBS := c sel4 elf cpio sel4muslcsys sel4allocman sel4utils sel4platsupport sel4bench sel4simple utils platsupport sel4vka sel4vspace
ifdef CONFIG_KERNEL_STABLE
LIBS += sel4simple-stable
else
LIBS += sel4simple-default
endif

include $(SEL4_COMMON)/common.mk
