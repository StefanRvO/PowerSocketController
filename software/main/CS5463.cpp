#include "CS5463.h"
#include <cstring>
xSemaphoreHandle CS5463::spi_mux = xSemaphoreCreateMutex();
spi_device_handle_t CS5463::spi;
uint8_t CS5463::device_cnt;
bool CS5463::initialised;
gpio_num_t CS5463::miso_pin;
gpio_num_t CS5463::mosi_pin;
gpio_num_t CS5463::clk_pin;
#define SYNC0 0xFE
#define SYNC1 0xFF
CS5463::CS5463(gpio_num_t slave_select)
{
    __attribute__((unused)) esp_err_t ret;
    if(CS5463::initialised == false)
    {   //Init spi device.
        spi_bus_config_t buscfg;
        memset(&buscfg, 0, sizeof(buscfg));
        buscfg.miso_io_num = this->miso_pin;
        buscfg.mosi_io_num = this->mosi_pin;
        buscfg.sclk_io_num = this->clk_pin;
        buscfg.quadwp_io_num=-1;
        buscfg.quadhd_io_num=-1;
        ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
        CS5463::initialised = true;
    }
    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz=2000000,               //Clock out at 2 MHz
    devcfg.mode=0,                                //SPI mode 0
    devcfg.spics_io_num = slave_select,               //CS pin
    devcfg.queue_size = 10,
    devcfg.pre_cb = nullptr;
    devcfg.command_bits = 8;
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &this->spi);
}

void CS5463::set_spi_pins(gpio_num_t miso, gpio_num_t mosi, gpio_num_t clk)
{
    CS5463::miso_pin = miso;
    CS5463::mosi_pin = mosi;
    CS5463::clk_pin = clk;
}

int CS5463::do_spi_transaction(uint8_t cmd, uint8_t len, uint8_t *data_out, uint8_t *data_in)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = len;
    t.tx_buffer = data_out;
    t.rx_buffer = data_in;
    esp_err_t ret;
    xSemaphoreTake(this->spi_mux, portMAX_DELAY);
    ret = spi_device_transmit(this->spi, &t);  //Transmit!
    xSemaphoreGive(this->spi_mux);
    return ret;
}

int CS5463::read_register(registers the_register, uint8_t *databuff)
{
    uint8_t data_out[3] = {SYNC1, SYNC1, SYNC1};
    return this->do_spi_transaction(the_register << 1, sizeof(data_out), data_out, databuff);
}

int CS5463::write_register(registers the_register, uint8_t *data_out, uint8_t *data_in, uint8_t len)
{
    return this->do_spi_transaction(0x80 | (the_register << 1), len, data_out, data_in);
}

int CS5463::start_conversions(bool single_shot)
{ return do_spi_transaction(0xE0 | ~(single_shot & 0x7)); }

int CS5463::power_up_halt()
{ return do_spi_transaction(0xA0); }

int CS5463::software_reset()
{ return do_spi_transaction(0x80); }

int CS5463::halt_to_standby()
{ return do_spi_transaction(0x88); }

int CS5463::halt_to_sleep()
{ return do_spi_transaction(0x90); }

int CS5463::perform_calibration(calibration_type type)
{ return do_spi_transaction(0xC0 | type); }

int CS5463::read_status_register(status_register *data)
{
    int err;
    uint8_t data_out[3];
    err = this->read_register(registers::status, data_out);
    data->data_ready = data_out[0] & 0x80;
    data->conversion_ready = data_out[0] & 0x10;
    data->current_out_range = data_out[0] & 0x02;
    data->voltage_out_range = data_out[0] & 0x01;
    data->irms_out_range = data_out[1] & 0x40;
    data->vrms_out_range = data_out[1] & 0x20;
    data->energy_out_range = data_out[1] & 0x10;
    data->current_fault = data_out[1] & 0x08;
    data->voltage_sag = data_out[1] & 0x04;
    data->temp_update = data_out[2] & 0x80;
    data->temp_modu_oscilation = data_out[2] & 0x40;
    data->voltage_modu_oscilation = data_out[2] & 0x10;
    data->current_modu_oscilation = data_out[2] & 0x08;
    data->low_supply_detected = data_out[2] & 0x04;
    data->epsilon_update = data_out[2] & 0x02;
    data->invalid_command = data_out[2] & 0x01;
    return err;
}

int CS5463::read_temperature(float *temp)
{
    int err;
    uint8_t data_out[3];
    err = this->read_register(registers::status, data_out);
    *temp = (*(int8_t *)data_out);
    *temp += (*(uint16_t *)data_out + 1);
    return err;
}
