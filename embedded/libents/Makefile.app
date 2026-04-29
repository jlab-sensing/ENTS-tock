# Include headers from root
override CPPFLAGS += -I$(TOCK_USERLAND_BASE_DIR)/../libents

# Add micrlog headers
override CPPFLAGS += -I$(TOCK_USERLAND_BASE_DIR)/../libents/microlog/include
