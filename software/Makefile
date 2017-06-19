#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
CFLAGS += ${ALL_FLAGS}

CXXFLAGS += ${ALL_FLAGS}


PROJECT_NAME := PowerSocket
SPIFFS_ADDR := 0x220000
MKSPIFFS_DIR := build_tools/Lua-RTOS-ESP32/components/mkspiffs/src

include $(IDF_PATH)/make/project.mk

mkspiffs:
	echo "+---------------------+"
	echo "| Compiling mkspiffs  |"
	echo "+---------------------+"
	cd ${MKSPIFFS_DIR} && make && cp ./mkspiffs ${BUILD_DIR_BASE}/

images: mkspiffs
	echo "Resolving server side includes"
	cp -R -T spiffs_files ${BUILD_DIR_BASE}/spiffs_files/
	cd ${BUILD_DIR_BASE}/spiffs_files/html/; bash ${PROJECT_PATH}/SSI_parser.sh
	echo "+---------------------+"
	echo "| Building spiffs.img |"
	echo "+---------------------+"
	${BUILD_DIR_BASE}/mkspiffs -c ${BUILD_DIR_BASE}/spiffs_files -b 65536 -p 256 -s 0x100000 ${BUILD_DIR_BASE}/spiffs.img
fat_images:
	echo "Resolving server side includes"
	cp -R -T spiffs_files ${BUILD_DIR_BASE}/spiffs_files/
	cd ${BUILD_DIR_BASE}/spiffs_files/html/; bash ${PROJECT_PATH}/SSI_parser.sh
	echo "+---------------------+"
	echo "| Building fatfs.img |"
	echo "+---------------------+"
	dd if=/dev/zero of=${BUILD_DIR_BASE}/fatfs.img count=1 bs=1M
	mkfs.vfat ${BUILD_DIR_BASE}/fatfs.img
	mcopy -si ${BUILD_DIR_BASE}/fatfs.img ${BUILD_DIR_BASE}/spiffs_files/* ::/

flashdata: images
	echo "Flashing SPIFFS to ESP32"
	$(ESPTOOLPY_WRITE_FLASH) --compress ${SPIFFS_ADDR} build/spiffs.img

flashall: flash flashdata

release: all images
	echo "+---------------------+"
	echo "| Creating release    |"
	echo "+---------------------+"
	mkdir -p ${BUILD_DIR_BASE}/release
	cp ${BUILD_DIR_BASE}/partitions.bin ${BUILD_DIR_BASE}/PowerSocket.bin ${BUILD_DIR_BASE}/spiffs.img ${BUILD_DIR_BASE}/release/
	tar -zcvf ${BUILD_DIR_BASE}/release.tar.gz -C ${BUILD_DIR_BASE}/release .
