#pragma once
#include "driver/gpio.h"
#include <cstdint>
void i2cscanner(gpio_num_t scl, gpio_num_t sda, uint8_t **i2c_devices, uint8_t *device_count);
