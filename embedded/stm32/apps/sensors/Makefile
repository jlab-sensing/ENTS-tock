# Makefile for user application

# Specify this directory relative to the current application.
TOCK_USERLAND_BASE_DIR = ../../../libtock-c

PACKAGE_NAME = org.ents.apps.sensors

# increase stack size
STACK_SIZE=4096
# decrease heap size from default
APP_HEAP_SIZE=4096

EMBED_DIR = $(TOCK_USERLAND_BASE_DIR)/..
EXT_DIR = $(EMBED_DIR)/external

EXTERN_LIBS += $(EMBED_DIR)/libents
EXTERN_LIBS += $(EXT_DIR)/bme280
EXTERN_LIBS += $(EXT_DIR)/microlog
EXTERN_LIBS += $(EXT_DIR)/nanopb

# Keep the TBF at 32 KiB so it fits in the 32 KiB left after core (128 KiB) in
# app flash. libtock-c's default 3000 byte footer pushes the app just past
# 32 KiB, which elf2tab then rounds up to 64 KiB.
TBF_NO_FOOTER = 1
ELF2TAB_ARGS += --minimum-footer-size 1024

# Which files to compile.
C_SRCS := main.c

# Include userland master makefile. Contains rules and flags for actually
# building the application.
include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
