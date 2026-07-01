# Makefile for FRAM test application

# Specify this directory relative to the current application.
TOCK_USERLAND_BASE_DIR = ../../../libtock-c

EMBED_DIR = $(TOCK_USERLAND_BASE_DIR)/..
EXT_DIR = $(EMBED_DIR)/external

EXTERN_LIBS += $(EMBED_DIR)/libents
EXTERN_LIBS += $(EXT_DIR)/unity
EXTERN_LIBS += $(EXT_DIR)/nanopb


# Which files to compile.
C_SRCS := main.c

STACK_SIZE := 4096

# Include userland master makefile. Contains rules and flags for actually
# building the application.

include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
