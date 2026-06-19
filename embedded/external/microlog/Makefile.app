# Add headers
override CPPFLAGS += -isystem $(TOCK_USERLAND_BASE_DIR)/../external/microlog/microlog/include

# Flags
override CPPFLAGS += -DTEMPLATE
