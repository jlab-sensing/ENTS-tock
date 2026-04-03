TOCK_USERLAND_BASE_DIR ?= ../../libtock-c
LIBNAME := bme280

# Careful! Must be a path that resolves correctly **from where make is invoked**
#
# If you are only ever compiling a standalone library, then it's fine to simply set
$(LIBNAME)_DIR := $(TOCK_USERLAND_BASE_DIR)/../external/$(LIBNAME)
#$(LIBNAME)_DIR := .

# If you will be asking applications to rebuild this library (see the development
# section below), then you'll need to ensure that this directory is still correct
# when invoked from inside the application folder.
#
# Tock accomplishes this for in-tree libraries by having all makefiles
# conditionally set the TOCK_USERLAND_BASE_DIR variable, so that there
# is a common relative path everywhere.
#$(LIBNAME)_DIR := $(TOCK_USERLAND_BASE_DIR)/$(LIBNAME)

SRC_DIR = $($(LIBNAME)_DIR)

$(LIBNAME)_SRCS := $(wildcard $(SRC_DIR)/BME280_SensorAPI/*.c)
$(LIBNAME)_SRCS += $(SRC_DIR)/bme280_common.c

override CPPFLAGS_$(LIBNAME) += -isystem $(SRC_DIR)
override CPPFLAGS_$(LIBNAME) += -isystem $(SRC_DIR)/BME280_SensorAPI
override CPPFLAGS_$(LIBNAME) += -DBME280_32BIT_ENABLE=1

include $(TOCK_USERLAND_BASE_DIR)/TockLibrary.mk
