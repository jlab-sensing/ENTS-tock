# Makefile for FRAM test application

# Specify this directory relative to the current application.
TOCK_USERLAND_BASE_DIR = ../../../libtock-c

EXTERN_LIBS += ../unity
EXTERN_LIBS += ../../../libents

# Which files to compile.
C_SRCS := main.c

STACK_SIZE := 2048

# Include userland master makefile. Contains rules and flags for actually
# building the application.

include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
