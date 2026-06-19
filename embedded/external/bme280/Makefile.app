# Add headers
override CPPFLAGS += -isystem $(TOCK_USERLAND_BASE_DIR)/../external/bme280
override CPPFLAGS += -isystem $(TOCK_USERLAND_BASE_DIR)/../external/bme280/BME280_SensorAPI

# Set 32-bit support
override CPPFLAGS += -D BME280_32BIT_ENABLE
