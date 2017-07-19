#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "i2cscanner.h"

void i2cscanner(gpio_num_t scl, gpio_num_t sda, uint8_t **i2c_devices, uint8_t *device_count) {
	i2c_config_t conf;
	conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = sda;
	conf.scl_io_num = scl;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	conf.master.clk_speed = 100000;
	i2c_param_config(I2C_NUM_0, &conf);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    *device_count = 0;
    *i2c_devices = nullptr;
	int i;
	esp_err_t espRc;
	for (int i = 3; i <  0x78; i++) {
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, 1 /* expect ack */);
		i2c_master_stop(cmd);

		espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
		if (espRc == 0) {
            (*device_count)++;
            if(device_count == 1)
                *i2c_devices = (uint8_t *)malloc(sizeof(**i2c_devices));
            else
                *i2c_devices = (uint8_t *)realloc(*i2c_devices, sizeof(**i2c_devices) * *device_count);
			(*i2c_devices)[*device_count - 1] = i;
		}
		i2c_cmd_link_delete(cmd);
	}
    i2c_driver_delete(I2C_NUM_0);
}
