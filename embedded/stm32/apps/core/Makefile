# Makefile for user application

# Specify this directory relative to the current application.
TOCK_USERLAND_BASE_DIR = ../../../libtock-c

PACKAGE_NAME = org.ents.core

STACK_SIZE=8192
APP_HEAP_SIZE=4096

EMBED_DIR = $(TOCK_USERLAND_BASE_DIR)/..
EXT_DIR = $(EMBED_DIR)/external

EXTERN_LIBS += $(EMBED_DIR)/libents

EXTERN_LIBS += $(EXT_DIR)/bme280
EXTERN_LIBS += $(EXT_DIR)/microlog
EXTERN_LIBS += $(EXT_DIR)/nanopb

EXTERN_LIBS += $(TOCK_USERLAND_BASE_DIR)/RadioLib

# Option to use static parameters for dev eui
LORAWAN_DEV ?= 0
ifeq ($(LORAWAN_DEV), 1)
	override CPPFLAGS += -DSTATIC_DEV_EUI
	override CPPFLAGS += -DRADIOLIB_LORAWAN_APP_KEY=02,B0,7E,01,50,16,02,80,AE,0D,20,A6,0A,B0,F7,01
	override CPPFLAGS += -DRADIOLIB_LORAWAN_NWK_KEY=B8,2D,91,3F,AF,B0,99,EE,2E,3B,BE,9B,31,96,C1,09
endif

# Pad the TBF so the Cortex-M MPU can protect the app's flash. Apps start at
# 0x08018000, which is only 32 KiB aligned, so the kernel needs the TBF size to
# be a multiple of 32 KiB. elf2tab only rounds to 16 KiB below 128 KiB, so a
# larger footer (replacing libtock-c's default of 3000 bytes) pushes the app
# to 128 KiB. Above 128 KiB elf2tab rounds to 32 KiB on its own.
TBF_NO_FOOTER = 1
ELF2TAB_ARGS += --minimum-footer-size 20480

# ignore old () rather than reinterpret because we are forced to compile cc
# from RadioLib
override CPPFLAGS += -Wno-old-style-cast

# Which files to compile.
# Excluding demangle_stub.cc will remove a size optimization that disables run
# time c++ name mangling for exception printing.
CXX_SRCS := user_config.cc lorawan.cc main.cc demangle_stub.cc

# Include userland master makefile. Contains rules and flags for actually
# building the application.
include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
