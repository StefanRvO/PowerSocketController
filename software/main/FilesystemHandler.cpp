#include "FilesystemHandler.h"
extern "C"
{
    #include "spiffs_vfs.h"
    #include "esp_spiffs.h"
}
FilesystemHandler::FilesystemHandler(size_t _addr, size_t _size, char * _mountpt)
:addr(_addr), size(_size), mountpt(_mountpt)
{}

bool FilesystemHandler::init_spiffs()
{
    //Configure the blocksizes and addresses of the spiffs.
    //This should correlate with the settings in the makefile and partitions.csv
	spiffs_config cfg;
    int rc;
	cfg.phys_size = this->size;
	cfg.phys_addr = this->addr;
	cfg.phys_erase_block = 65536;
	cfg.log_block_size = 65536;
	cfg.log_page_size = LOG_PAGE_SIZE;

    //Setup spi flash functions
	cfg.hal_read_f = esp32_spi_flash_read;
	cfg.hal_write_f = esp32_spi_flash_write;
    cfg.hal_erase_f = esp32_spi_flash_erase;

    //Try to mount
    printf("Loading SPIFFS at address 0x%x", cfg.phys_addr);
	rc = SPIFFS_mount(&this->fs,
		&cfg,
		this->spiffs_work_buf,
		this->spiffs_fds,
		sizeof(this->spiffs_fds),
		this->spiffs_cache_buf,
		sizeof(this->spiffs_cache_buf),
		0);
	if (rc != 0) {
        printf("Error mounting SPIFFS! (have you flashed it to the correct address?)\n");
        printf("This was the SPIFFS settings used:\n");
        printf("Address: 0x%x\n", cfg.phys_addr);
        printf("Size: 0x%x\n", cfg.phys_size);
        printf("Mount point: %s", this-> mountpt);
        printf("This will probably cause major problems.");
        return false;
    }
    spiffs_registerVFS(this->mountpt, &this->fs);
    printf("Filesystem initialsation completed!");
    return true;
}
