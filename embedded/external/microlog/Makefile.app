# Add headers
override CPPFLAGS += -isystem $(TOCK_USERLAND_BASE_DIR)/../external/microlog/microlog/include

# Flags
override CPPFLAGS += -DULOG_BUILD_PREFIX_SIZE=32
