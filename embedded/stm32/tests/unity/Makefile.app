# Disable floating point support in Unity to avoid -Wfloat-equal warnings 
# which cause CI failures due to -Werror.
override CPPFLAGS += -I$(TOCK_USERLAND_BASE_DIR)/../stm32/tests/unity -DUNITY_EXCLUDE_FLOAT -DUNITY_EXCLUDE_DOUBLE
