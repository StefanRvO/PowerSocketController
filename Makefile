# Simple makefile for simple example

PROGRAM=simple
PROGRAM_SRC_DIR = src

PROGRAM_INC_DIR = include

CFLAGS += "-std=gnu99"
CFLAGS += "-fno-inline-functions"
CFLAGS += "-nostdlib"
CFLAGS += "-mlongcalls"
CFLAGS += "-mtext-section-literals"
CFLAGS += "-Wno-address"
CFLAGS += "-g"
CFLAGS += "-O2"
CFlags += "-ffunction-sections"
CFLAGS += "-fdata-sections"



CFLAGS += "-ggdb"
#CFLAGS += "-Werror"
CFLAGS += "-Wpointer-arith"
CFLAGS += "-Wall"
CFLAGS += "-Wl,-EL"
CFLAGS += "-fno-inline-functions"
CFLAGS += "-nostdlib"
CFLAGS += "-mlongcalls"
CFLAGS += "-mtext-section-literals"
CFLAGS += "-D__ets__"
CFLAGS += "-DICACHE_FLASH"
CFLAGS += "-DHTTPD_MAX_CONNECTIONS=4"
CFLAGS += "-DHTTPD_STACKSIZE=2048"
CFLAGS += "-DUSE_OPENSDK"
CFLAGS += "-Wno-address"
CFLAGS += "-DESPFS_HEATSHRINK"
CFLAGS += "-DHTTPD_WEBSOCKETS"
CFLAGS += "-DDEBUG=1"

CFLAGS += "-DUSE_OPENSDK"
CFLAGS += "-DFREERTOS"
CFLAGS += "-DLWIP_OPEN_SRC"
CFLAGS += "-ffunction-sections"
CFLAGS += "-fdata-sections"
CFLAGS += "-DFREERTOS"
CFLAGS += "-D__GNU_VISIBLE"

CFLAGS += "-Dos_printf=printf"




C_CXX_FLAGS += "-std=c++11"
C_CXX_FLAGS += "-fpermissive"
LIBS += "hal"
LIBS += "gcc"
LIBS += "c"
LIBS += "stdc++"
LIBS += "supc++"



ESPBAUD= 921600
ESPPORT=/dev/ttyUSB0
FLASH_SIZE=32
EXTRA_COMPONENTS = extras/spiffs extras/rboot-ota extras/libesphttpd

SPIFFS_BASE_ADDR = 0x300000
SPIFFS_SIZE = 0x10000

include ../../common.mk

$(eval $(call make_html_image))

$(eval $(call make_spiffs_image,files))
