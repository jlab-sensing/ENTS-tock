# Makefile for user application

# Specify this directory relative to the current application.
TOCK_USERLAND_BASE_DIR = ../../libtock-c

PACKAGE_NAME = org.ents.core

STACK_SIZE=4096
APP_HEAP_SIZE=4096

EXTERN_LIBS += ../../libents
EXTERN_LIBS += $(TOCK_USERLAND_BASE_DIR)/RadioLib

# Which files to compile.
CXX_SRCS := $(wildcard *.cc)
#CXX_SRCS := main.cc

# Include userland master makefile. Contains rules and flags for actually
# building the application.
include $(TOCK_USERLAND_BASE_DIR)/AppMakefile.mk
