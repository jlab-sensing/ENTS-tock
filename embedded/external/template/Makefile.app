# Add headers
override CPPFLAGS += -isystem $(TOCK_USERLAND_BASE_DIR)/../external/template

# Set 32-bit support
override CPPFLAGS += -DTEMPLATE
