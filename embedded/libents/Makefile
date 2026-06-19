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

SRC_DIR = $($(LIBNAME)_DIR)/src/$(LIBNAME)

# Or let make find them automatically:
$(LIBNAME)_SRCS := $(wildcard $(SRC_DIR)/controller/*.c)
$(LIBNAME)_SRCS += $(wildcard $(SRC_DIR)/controller/modules/*.c)
$(LIBNAME)_SRCS += $(wildcard $(SRC_DIR)/proto/*.c)
$(LIBNAME)_SRCS += $(wildcard $(SRC_DIR)/sensors/*.c)
$(LIBNAME)_SRCS += $(wildcard $(SRC_DIR)/sensors/bme280/*.c)
$(LIBNAME)_SRCS += $(wildcard $(SRC_DIR)/storage/*.c)
$(LIBNAME)_SRCS += $(wildcard $(SRC_DIR)/util/*.c)
$(LIBNAME)_SRCS += $(SRC_DIR)/user_config.c

# Sources for nanopb
# Wasn't able to have a library require another library so having nanopb inside
# the libents source tree
$(LIBNAME)_SRCS += $(SRC_DIR)/nanopb/pb_common.c
$(LIBNAME)_SRCS += $(SRC_DIR)/nanopb/pb_decode.c
$(LIBNAME)_SRCS += $(SRC_DIR)/nanopb/pb_encode.c

# Sources for microlog
$(LIBNAME)_SRCS += $(SRC_DIR)/microlog/src/ulog.c

#$(LIBNAME)_SRCS := $(wildcard $($(LIBNAME)_DIR)/controller*.c)
#$(LIBNAME)_SRCS  += $(wildcard $($(LIBNAME)_DIR)/c.c)

override CPPFLAGS_$(LIBNAME) += -isystem $(SRC_DIR)/microlog/include
override CPPFLAGS_$(LIBNAME) += -DULOG_BUILD_PREFIX_SIZE=32

# add external headers
# NOTE This is basically a replication of each individual `Makefile.app`. There
# is no way to include an external library when building a library

EXT_DIR = $(TOCK_USERLAND_BASE_DIR)/../external/

# bme280
override CPPFLAGS_$(LIBNAME) += -isystem $(EXT_DIR)/bme280
override CPPFLAGS_$(LIBNAME) += -isystem $(EXT_DIR)/bme280/BME280_SensorAPI

include $(TOCK_USERLAND_BASE_DIR)/TockLibrary.mk
