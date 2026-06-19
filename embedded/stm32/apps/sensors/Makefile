# Makefile for user application

# Specify this directory relative to the current application.
TOCK_USERLAND_BASE_DIR = ../../../libtock-c

PACKAGE_NAME = org.ents.apps.sensors

# increase stack size
STACK_SIZE=4096
# decrease heap size from default
APP_HEAP_SIZE=4096

LIBS_DIR = $(TOCK_USERLAND_BASE_DIR)/..
EXT_DIR = $(LIBS_DIR)/external

EXTERN_LIBS += $(LIBS_DIR)/libents
EXTERN_LIBS += $(EXT_DIR)/bme280

# Which files to compile.
C_SRCS := main.c

# Include userland master makefile. Contains rules and flags for actually
# building the application.
include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
