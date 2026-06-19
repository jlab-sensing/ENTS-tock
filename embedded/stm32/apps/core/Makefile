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


# Which files to compile.
#CXX_SRCS := $(wildcard *.cc)
#CXX_SRCS += $(wildcard *c)
#CXX_SRCS := main.cc lorawan.cc user_config.c
#CXX_SRCS := main2.cc
CXX_SRCS := user_config.cc lorawan.cc main.cc
# Include userland master makefile. Contains rules and flags for actually
# building the application.
include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
