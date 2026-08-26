# Makefile for FIFO insertion test application

TOCK_USERLAND_BASE_DIR = ../../../libtock-c

PACKAGE_NAME = org.ents.fifo_insert

STACK_SIZE=4096
APP_HEAP_SIZE=2048

EMBED_DIR = $(TOCK_USERLAND_BASE_DIR)/..
EXT_DIR = $(EMBED_DIR)/external

EXTERN_LIBS += $(EMBED_DIR)/libents
EXTERN_LIBS += $(EXT_DIR)/microlog
EXTERN_LIBS += $(EXT_DIR)/nanopb

C_SRCS := $(wildcard *.c)

include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
