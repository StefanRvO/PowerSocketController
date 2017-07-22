#include "CS5463.h"
#include <cstring>
xSemaphoreHandle CS5463::spi_mux;
bool CS5463::initialised = false;
#define SYNC0 0xFE
#define SYNC1 0xFF

static void callback(spi_transaction_t *t)
{
    //printf("Transmit\n");
}

CS5463::CS5463(gpio_num_t slave_select)
{
    printf("Creating device with CS %d\n", slave_select);
    static spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz=2000000,               //Clock out at 2 MHz
    devcfg.mode=0,                                //SPI mode 0
    devcfg.spics_io_num = slave_select,               //CS pin
    devcfg.queue_size = 1,
    devcfg.pre_cb = nullptr;
    devcfg.command_bits = 8;
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &this->spi));
}

void CS5463::init_spi(gpio_num_t miso, gpio_num_t mosi, gpio_num_t clk)
{
    if(CS5463::initialised == false)
    {   //Init spi device.
        printf("initialising SPI bus\n");
        static spi_bus_config_t buscfg;
        memset(&buscfg, 0, sizeof(buscfg));
        buscfg.miso_io_num = miso;
        buscfg.mosi_io_num = mosi;
        buscfg.sclk_io_num = clk;
        buscfg.quadwp_io_num=-1;
        buscfg.quadhd_io_num=-1;
        ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 0));
        CS5463::initialised = true;
        CS5463::spi_mux = xSemaphoreCreateMutex();
    }
    else
        printf("SPI already initialised");
}

int CS5463::do_spi_transaction(uint8_t cmd, uint8_t len, uint8_t *data_out, uint8_t *data_in)
{
    spi_transaction_t t;
    printf("cmd: 0x%.2x, len: %d\n", cmd, len);
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = len * 8;
    //if (data_in)t.rxlength = 4 * 8;
    t.tx_buffer = data_out;
    t.rx_buffer = data_in;
    t.command = cmd;
    esp_err_t ret;
    printf("test\n");
    xSemaphoreTake(this->spi_mux, portMAX_DELAY);
    printf("test2\n");
    ret = spi_device_transmit(this->spi, &t);  //Transmit!
    printf("test3\n");
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
    return this->do_spi_transaction(0x40 | (the_register << 1), len, data_out, data_in);
}

int CS5463::start_conversions(bool single_shot)
{
    if(single_shot) return do_spi_transaction(0xE0);
    else            return do_spi_transaction(0xE8);
}

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
    uint8_t data_out[4];
    err = this->read_register(registers::status, data_out);
    printf("0x%.2x%.2x%.2x\n", data_out[0], data_out[1], data_out[2]);
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
    uint8_t data_out[4];
    err = this->read_register(registers::temperature, data_out);
    printf("0x%.2x%.2x%.2x\n", data_out[0], data_out[1], data_out[2]);
    *temp = (*(int8_t *)data_out);
    *temp += data_out[1] / float(1 << 7);
    *temp += data_out[2] / float(1 << 15);
    return err;
}


int CS5463::set_computation_cycle_duration(uint32_t milliseconds) //Assume MCLK = 4.096 MHz
{
    uint32_t N = (((uint64_t)4000) * ((uint64_t)milliseconds)) / 1000;
    uint8_t data_out[3] = { uint8_t((N >> 16) & 0xFF), uint8_t((N >> 8) & 0xFF), uint8_t((N) & 0xFF)};
    this->owr = N;
    return this->write_register(registers::cycle_count, data_out);
}

int CS5463::set_operation_mode(mode_register reg) //Assume MCLK = 4.096 MHz
{
    uint8_t data_out[3] =  { 0 };
    data_out[2] |= reg.e2mode << 1;
    data_out[2] |= reg.xvdel;
    data_out[3] |= reg.xidel << 7;
    data_out[3] |= reg.ihpf << 6;
    data_out[3] |= reg.vhpf << 5;
    data_out[3] |= reg.iir << 4;
    data_out[3] |= reg.e3mode << 2;
    data_out[3] |= reg.pos << 1;
    data_out[3] |= reg.afc;
    return this->write_register(registers::mode, data_out);
}

int CS5463::set_epsilon(float e)
{
    uint8_t data_out[3] =  { 0xFF, 0xFF, 0xFF };
    int32_t tmp = (e * (1 << 23));
    data_out[0] = tmp >> 15;
    data_out[1] = (tmp >> 7) & 0xFF;
    data_out[2] = (tmp) & 0xFF;
    return this->write_register(registers::epsilon, data_out);
}

int CS5463::get_epsilon(float *e)
{
    uint8_t data_out[4] =  { 0 };
    int err = this->read_register(registers::epsilon, data_out);
    printf("0x%.2x%.2x%.2x\n", data_out[0], data_out[1], data_out[2]);
    uint32_t reg_val = data_out[0] + (data_out[1] << 7) + (data_out[2] << 15);
    *e = (reg_val) / float(1 << 23);
    return err;
}
