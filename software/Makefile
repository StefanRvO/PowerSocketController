#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
CFLAGS += ${ALL_FLAGS}

CXXFLAGS += ${ALL_FLAGS}
CXXFLAGS += -std=c++11

PROJECT_NAME := software
export LWS_IS_FACTORY_APPLICATION
include $(IDF_PATH)/make/project.mk
include sdkconfig
include ${PWD}/components/libwebsockets/scripts/esp32.mk
CFLAGS +=-I$(PROJECT_PATH)/components/libwebsockets/plugins
CXXFLAGS +=-I$(PROJECT_PATH)/components/libwebsockets/plugins
GENCERTS = 0

HOST = 192.168.200.90


romfs_prepare:
	echo "Resolving server side includes"
	cp -R -T data_files ${BUILD_DIR_BASE}/data_files/
	cd ${BUILD_DIR_BASE}/data_files/html/; bash ${PROJECT_PATH}/SSI_parser.sh
	cd ${BUILD_DIR_BASE}/data_files/html/; zip -9 -r ${BUILD_DIR_BASE}/data_files/html.zip .
	rm -Rf ${BUILD_DIR_BASE}/data_files/html/
	echo "+---------------------------------------------------+"
	echo "| Copying to romfs_files as libwebsockets want this |"
	echo "+---------------------------------------------------+"
	rm -Rf $(PROJECT_PATH)/romfs-files
	cp -R -T ${BUILD_DIR_BASE}/data_files/ $(PROJECT_PATH)/romfs-files
$(APP_BIN): romfs_prepare

release: all
	echo "+---------------------+"
	echo "| Creating release    |"
	echo "+---------------------+"
	mkdir -p ${BUILD_DIR_BASE}/release
	cp ${BUILD_DIR_BASE}/partitions.bin ${BUILD_DIR_BASE}/software.bin ${BUILD_DIR_BASE}/release/
	tar -zcvf ${BUILD_DIR_BASE}/release.tar.gz -C ${BUILD_DIR_BASE}/release .

ota_flash: all
	echo "+------------------------+"                     
	echo "| Flashing Over The Air! |"                     
	echo "+------------------------+"
	$(PROJECT_PATH)/Tools/OTAUploader.py -f  ${BUILD_DIR_BASE}/software.bin -ht ${HOST}
