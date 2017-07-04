#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

CXXFLAGS += -Wno-reorder
CXXFLAGS +=-Wno-switch
CFLAGS += ${ALL_FLAGS}
CXXFLAGS += ${ALL_FLAGS}
COMPONENT_DEPENDS:=libwebsockets
