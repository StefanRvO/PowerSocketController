#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ALL_FLAGS = -Wno-reorder

CFLAGS += ${ALL_FLAGS}
CXXFLAGS += ${ALL_FLAGS}
