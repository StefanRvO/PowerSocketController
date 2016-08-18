# Simple makefile for simple example

PROGRAM=simple
PROGRAM_SRC_DIR = src

CFLAGS += "-std=gnu99"
CFLAGS += "-g"
CFLAGS += "-O2"
CFlags += "-ffunction-sections"
CFLAGS += "-fdata-sections"
#CFLAGS += "-I headers"
C_CXX_FLAGS += "-I headers"
C_CXX_FLAGS += "-std=c++11"

LIBS += "hal"
LIBS += "gcc"
LIBS += "c"
LIBS += "stdc++"
LIBS += "supc++"



ESPBAUD= 460800
ESPPORT=/dev/ttyUSB0
FLASH_SIZE=32
include ../../common.mk
