# Base definitions
TOCK_USERLAND_BASE_DIR ?= ../../../libtock-c
LIBNAME := unity

# Careful! Must be a path that resolves correctly **from where make is invoked**
#
# If you are only ever compiling a standalone library, then it's fine to simply set
$(LIBNAME)_DIR := $(TOCK_USERLAND_BASE_DIR)/../stm32/tests/$(LIBNAME)
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
$(LIBNAME)_SRCS := $(wildcard $($(LIBNAME)_DIR)/*.c)

#$(LIBNAME)_SRCS := $(wildcard $($(LIBNAME)_DIR)/controller*.c)
#$(LIBNAME)_SRCS  += $(wildcard $($(LIBNAME)_DIR)/c.c)

include $(TOCK_USERLAND_BASE_DIR)/TockLibrary.mk
