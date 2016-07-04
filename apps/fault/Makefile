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
LIBS := sel4 c elf cpio utils sel4utils sel4allocman sel4vspace sel4simple \
	    platsupport sel4platsupport sel4bench sel4vka sel4benchsupport \
		sel4muslcsys sel4debug

include $(SEL4_COMMON)/common.mk
