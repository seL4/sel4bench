#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_GPL)
#

# Targets
TARGETS := $(notdir $(SOURCE_DIR)).bin

# Set custom entry point as we are a rootserver and will not be
# started in a standard way
ENTRY_POINT := _sel4_start

# Source files required to build the target
CFILES   := $(sort $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c)))
CFILES   += $(sort $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/plat/$(PLAT)/*.c)))
CFILES   += $(sort $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/arch/$(ARCH)/*.c)))

# CPIO archive
OFILES := archive.o

# We want to run C99
CFLAGS += -std=gnu99

# Optimize for size
CFLAGS += -Os

# Libraries required to build the target
LIBS = c sel4 elf cpio sel4muslcsys sel4allocman sel4utils sel4platsupport sel4bench sel4simple \
	    utils platsupport sel4vka sel4vspace sel4debug sel4benchsupport jansson sel4serialserver
LIBS += sel4simple-default

include $(SEL4_COMMON)/common.mk

# targets to generate CPIO archive of elf files
${COMPONENTS}:
	false

archive.o: ${COMPONENTS}
	$(Q)mkdir -p $(dir $@)
	${COMMON_PATH}/files_to_obj.sh $@ _cpio_archive $^
