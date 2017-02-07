#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
ALL_FLAGS += -Wno-unused-function
ALL_FLAGS += -DMG_ENABLE_FILESYSTEM
ALL_FLAGS += -g

CFLAGS += ${ALL_FLAGS}

CXXFLAGS += ${ALL_FLAGS}


PROJECT_NAME := PowerSocket
SPIFFS_ADDR := 0x210000

include $(IDF_PATH)/make/project.mk

images:
	echo "+---------------------+"
	echo "| Building spiffs.img |"
	echo "+---------------------+"
	mkspiffs -c ./spiffs_files -b 65536 -p 256 -s 0x100000 build/spiffs.img

flashdata: images
	echo "Flashing SPIFFS to ESP32"
	$(ESPTOOLPY_WRITE_FLASH) --compress ${SPIFFS_ADDR} build/spiffs.img

flashall: flash flashdata
