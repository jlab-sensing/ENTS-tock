# Makefile for FRAM test application

# Specify this directory relative to the current application.
TOCK_USERLAND_BASE_DIR = ../../../libtock-c/

# Which files to compile.
C_SRCS := main.c mb85rc1mt.c unity.c

STACK_SIZE := 2048

# Include userland master makefile. Contains rules and flags for actually
# building the application.

# Disable floating point support in Unity to avoid -Wfloat-equal warnings 
# which cause CI failures due to -Werror.
override CFLAGS += -DUNITY_EXCLUDE_FLOAT -DUNITY_EXCLUDE_DOUBLE

include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
