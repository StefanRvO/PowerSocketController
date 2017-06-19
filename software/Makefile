#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
CFLAGS += ${ALL_FLAGS}

CXXFLAGS += ${ALL_FLAGS}


PROJECT_NAME := PowerSocket
FATFS_ADDR := 0x220000

include $(IDF_PATH)/make/project.mk


images:
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
	$(ESPTOOLPY_WRITE_FLASH) --compress ${FATFS_ADDR} build/fatfs.img

flashall: flash flashdata

release: all images
	echo "+---------------------+"
	echo "| Creating release    |"
	echo "+---------------------+"
	mkdir -p ${BUILD_DIR_BASE}/release
	cp ${BUILD_DIR_BASE}/partitions.bin ${BUILD_DIR_BASE}/PowerSocket.bin ${BUILD_DIR_BASE}/spiffs.img ${BUILD_DIR_BASE}/release/
	tar -zcvf ${BUILD_DIR_BASE}/release.tar.gz -C ${BUILD_DIR_BASE}/release .
