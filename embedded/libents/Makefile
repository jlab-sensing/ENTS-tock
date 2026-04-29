# Base definitions
TOCK_USERLAND_BASE_DIR ?= ../libtock-c
LIBNAME := libents

# Careful! Must be a path that resolves correctly **from where make is invoked**
#
# If you are only ever compiling a standalone library, then it's fine to simply set
$(LIBNAME)_DIR := $(TOCK_USERLAND_BASE_DIR)/../$(LIBNAME)
#$(LIBNAME)_DIR := .

# If you will be asking applications to rebuild this library (see the development
# section below), then you'll need to ensure that this directory is still correct
# when invoked from inside the application folder.
#
# Tock accomplishes this for in-tree libraries by having all makefiles
# conditionally set the TOCK_USERLAND_BASE_DIR variable, so that there
# is a common relative path everywhere.
#$(LIBNAME)_DIR := $(TOCK_USERLAND_BASE_DIR)/$(LIBNAME)

# Or let make find them automatically:
$(LIBNAME)_SRCS := $(wildcard $($(LIBNAME)_DIR)/controller/*.c)
$(LIBNAME)_SRCS += $(wildcard $($(LIBNAME)_DIR)/proto/*.c)
$(LIBNAME)_SRCS += $(wildcard $($(LIBNAME)_DIR)/storage/*.c)

# Sources for nanopb
# Wasn't able to have a library require another library so having nanopb inside
# the libents source tree
$(LIBNAME)_SRCS += $($(LIBNAME)_DIR)/nanopb/pb_common.c
$(LIBNAME)_SRCS += $($(LIBNAME)_DIR)/nanopb/pb_decode.c
$(LIBNAME)_SRCS += $($(LIBNAME)_DIR)/nanopb/pb_encode.c


#$(LIBNAME)_SRCS := $(wildcard $($(LIBNAME)_DIR)/controller*.c)
#$(LIBNAME)_SRCS  += $(wildcard $($(LIBNAME)_DIR)/c.c)

include $(TOCK_USERLAND_BASE_DIR)/TockLibrary.mk
