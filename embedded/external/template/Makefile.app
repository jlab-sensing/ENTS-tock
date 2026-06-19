# Add headers
override CPPFLAGS += -isystem $(TOCK_USERLAND_BASE_DIR)/../external/template

# Flags
override CPPFLAGS += -DTEMPLATE
