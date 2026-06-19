# Headers
override CPPFLAGS += -I$(TOCK_USERLAND_BASE_DIR)/../external/unity/Unity/src

# Disable floating point support in Unity to avoid -Wfloat-equal warnings 
# which cause CI failures due to -Werror.
override CPPFLAGS += -DUNITY_EXCLUDE_FLOAT
override CPPFLAGS += -DUNITY_EXCLUDE_DOUBLE
