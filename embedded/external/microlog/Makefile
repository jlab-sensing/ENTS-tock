# Base definitions
TOCK_USERLAND_BASE_DIR ?= ../../libtock-c
LIBNAME := microlog

# Careful! Must be a path that resolves correctly **from where make is invoked**
$(LIBNAME)_DIR := $(TOCK_USERLAND_BASE_DIR)/../external/$(LIBNAME)

SRC_DIR = $($(LIBNAME)_DIR)/microlog

# Use the same line with := replace with += to add additional sources
$(LIBNAME)_SRCS := $(SRC_DIR)/src/ulog.c

# Adding include paths, must use +=
override CPPFLAGS_$(LIBNAME) += -isystem $(SRC_DIR)/include
override CPPFLAGS_$(LIBNAME) += -DULOG_BUILD_PREFIX_SIZE=32

include $(TOCK_USERLAND_BASE_DIR)/TockLibrary.mk
