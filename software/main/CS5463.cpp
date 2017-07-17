#include "CS5463.h"

CS5463::CS5463(gpio_num_t slave_select)
{
    __attribute__((unused)) esp_err_t ret;
    if(CS5463::initialised == false)
    {   //Init spi device.
        spi_device_handle_t spi;
        spi_bus_config_t buscfg;
        buscfg.miso_io_num = this->miso_pin;
        buscfg.mosi_io_num = this->mosi_pin;
        buscfg.sclk_io_num = this->clk_pin;
        buscfg.quadwp_io_num=-1;
        buscfg.quadhd_io_num=-1;
        ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
        CS5463::initialised = true;
    }
    spi_device_interface_config_t devcfg;
    devcfg.clock_speed_hz=2000000,               //Clock out at 2 MHz
    devcfg.mode=0,                                //SPI mode 0
    devcfg.spics_io_num = slave_select,               //CS pin
    devcfg.queue_size = 10,
    devcfg.pre_cb = nullptr;
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &this->spi);

}

void CS5463::set_spi_pins(gpio_num_t miso, gpio_num_t mosi, gpio_num_t clk)
{
    CS5463::miso_pin = miso;
    CS5463::mosi_pin = mosi;
    CS5463::clk_pin = clk;
}
