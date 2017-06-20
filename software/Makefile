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
	cp -R -T data_files ${BUILD_DIR_BASE}/data_files/
	cd ${BUILD_DIR_BASE}/data_files/html/; bash ${PROJECT_PATH}/SSI_parser.sh
	echo "+---------------------+"
	echo "| Building fatfs.img |"
	echo "+---------------------+"
	#We have to be a bit hacky to build the flash image, as the wear leveling fucks shit up somewhat..
	dd if=${PROJECT_PATH}/flashdump skip=1 count=255 bs=4096 of=${BUILD_DIR_BASE}/raw_fat.img
	#Yeah sorry, i just overwrite your config file...
	echo "mtools_skip_check=1" > $(HOME)/.mtoolsrc
	mcopy -spmvi ${BUILD_DIR_BASE}/raw_fat.img ${BUILD_DIR_BASE}/data_files/* ::/
	#Creating final image
	dd if=${PROJECT_PATH}/flashdump count=1 bs=4096 of=${BUILD_DIR_BASE}/fatfs.img
	dd if=${BUILD_DIR_BASE}/raw_fat.img of=${BUILD_DIR_BASE}/fatfs.img obs=4096 seek=1
flashdata: images
	echo "Flashing SPIFFS to ESP32"
	$(ESPTOOLPY_WRITE_FLASH) --compress ${FATFS_ADDR} build/fatfs.img

flashall: flash flashdata

release: all images
	echo "+---------------------+"
	echo "| Creating release    |"
	echo "+---------------------+"
	mkdir -p ${BUILD_DIR_BASE}/release
	cp ${BUILD_DIR_BASE}/partitions.bin ${BUILD_DIR_BASE}/PowerSocket.bin ${BUILD_DIR_BASE}/fatfs.img ${BUILD_DIR_BASE}/release/
	tar -zcvf ${BUILD_DIR_BASE}/release.tar.gz -C ${BUILD_DIR_BASE}/release .
