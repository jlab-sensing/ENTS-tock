# Base definitions
TOCK_USERLAND_BASE_DIR ?= ../../libtock-c
LIBNAME := nanopb

# Careful! Must be a path that resolves correctly **from where make is invoked**
#
# If you are only ever compiling a standalone library, then it's fine to simply set
$(LIBNAME)_DIR := $(TOCK_USERLAND_BASE_DIR)/../external/$(LIBNAME)

SRC_DIR = $($(LIBNAME)_DIR)/nanopb

# Use the same line with := replace with += to add additional sources
$(LIBNAME)_SRCS := $(wildcard $(SRC_DIR)/*.c)

# Adding include paths, must use +=
override CPPFLAGS_$(LIBNAME) += -isystem $(SRC_DIR)

include $(TOCK_USERLAND_BASE_DIR)/TockLibrary.mk
