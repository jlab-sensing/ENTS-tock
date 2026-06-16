# Makefile for user application

# Specify this directory relative to the current application.
TOCK_USERLAND_BASE_DIR = ../../../libtock-c

PACKAGE_NAME = org.ents.apps.sensors

# increase stack size
STACK_SIZE=4096
# decrease heap size from default
APP_HEAP_SIZE=4096

EXTERN_LIBS += ../../../libents

# Which files to compile.
C_SRCS := main.c

# Include userland master makefile. Contains rules and flags for actually
# building the application.
include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
