# Include headers from root
override CPPFLAGS += -I $(TOCK_USERLAND_BASE_DIR)/../libents/src
# Add micrlog headers
override CPPFLAGS += -isystem $(TOCK_USERLAND_BASE_DIR)/../libents/src/libents/microlog/include
# Combile with 32-bit support
override CPPFLAGS += -D BME280_32BIT_ENABLE
