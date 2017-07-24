#include "CS5463.h"
#include <cstring>
xSemaphoreHandle CS5463::spi_mux;
bool CS5463::initialised = false;
#define SYNC0 0xFE
#define SYNC1 0xFF


int32_t three_bytes_to_int(uint8_t *bytes)
{
    uint32_t tmp = (((uint32_t)bytes[0]) << 24) | (((uint32_t)bytes[1]) << 16) | (((uint32_t)bytes[2]) << 8 );
    return *((int32_t *)&tmp) / (1 << 8);
}

uint32_t three_bytes_to_uint(uint8_t *bytes)
{
    return (((uint32_t)bytes[0]) << 16) | (((uint32_t)bytes[1]) << 8) | (((uint32_t)bytes[2]));
}

void uint_to_three_bytes(uint32_t n, uint8_t *bytes)
{
    bytes[0] = (n >> 16) & 0xFF;
    bytes[1] = (n >> 8) & 0xFF;
    bytes[2] = (n) & 0xFF;
}

void int_to_three_bytes(int32_t n, uint8_t *bytes)
{
    uint32_t tmp = *(uint32_t *)&n;
    bytes[0] = (tmp >> 16) & 0xFF;
    bytes[1] = (tmp >> 8) & 0xFF;
    bytes[2] = (tmp) & 0xFF;
    if(n < 0) bytes[0] |= 0x80;
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
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = len * 8;
    t.tx_buffer = data_out;
    t.rx_buffer = data_in;
    t.command = cmd;
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
    data_out[1] |= reg.e2mode << 1;
    data_out[1] |= reg.xvdel;
    data_out[2] |= reg.xidel << 7;
    data_out[2] |= reg.ihpf << 6;
    data_out[2] |= reg.vhpf << 5;
    data_out[2] |= reg.iir << 4;
    data_out[2] |= reg.e3mode << 2;
    data_out[2] |= reg.pos << 1;
    data_out[2] |= reg.afc;
    return this->write_register(registers::mode, data_out);
}

int CS5463::get_operation_mode(mode_register *reg) //Assume MCLK = 4.096 MHz
{
    uint8_t data[3] = {0};
    int err = this->read_register(registers::mode, data);
    reg->e2mode = data[1] & (1 << 1);
    reg->xvdel =  data[1] & 1;
    reg->xidel = data[2] & (1 << 7);
    reg->ihpf = data[2] & (1 << 6);
    reg->vhpf = data[2] & (1 << 5);
    reg->iir = data[2] & (1 << 4);
    reg->e3mode = (data[2] >> 2) & 0x03 ;
    reg->pos = data[2] & (1 << 1);
    reg->afc = data[2] & (1);
    return err;
}

int CS5463::set_frequency_measurement(bool afc)
{
    mode_register reg;
    int err = this->get_operation_mode(&reg);
    reg.afc = afc;
    err |= this->set_operation_mode(reg);
    return err;
}

int CS5463::read_temperature(float *temp)
{
    int err;
    uint8_t data_out[3];
    err = this->read_register(registers::temperature, data_out);
    int32_t reg_val = three_bytes_to_int(data_out);
    *temp = reg_val / float(1 << 16);
    return err;
}


int CS5463::set_epsilon(float e)
{
    uint8_t data_out[3] =  { 0xFF, 0xFF, 0xFF };
    int32_t tmp = (e * (1 << 23));
    data_out[0] = tmp >> 16;
    data_out[1] = (tmp >> 8) & 0xFF;
    data_out[2] = tmp & 0xFF;
    return this->write_register(registers::epsilon, data_out);
}

int CS5463::get_epsilon(float *e)
{
    uint8_t data_out[3] =  { 0 };
    int err = this->read_register(registers::epsilon, data_out);
    int32_t reg_val = three_bytes_to_int(data_out);
    printf("0x%.8x\n", reg_val);
    printf("0x%.2x%.2x%.2x\n", data_out[0], data_out[1], data_out[2]);

    *e = reg_val / float(1 << 23);
    return err;
}

int CS5463::set_control(ctrl_register reg)
{
    uint8_t data[3] = { 0 };
    data[1] = reg.stop;
    data[2] = (reg.int_opendrain << 4);
    data[2] |= (reg.no_cpu << 2);
    data[2] |= (reg.no_osc << 1);
    return this->write_register(registers::ctrl, data);
}


int CS5463::get_offset(registers type, float *result)
{   //Read an offset register.
    //Just call get_i_v_p_q_measurement, it does the same thing
    return this->get_i_v_p_q_measurement(type, result);
}

int CS5463::set_offset(registers type, float offset)
{
    uint8_t data[3] = { 0 };
    int_to_three_bytes(offset * (1 << 23), data);
    return this->write_register(type, data);
}

int CS5463::get_gain(registers type, float *result)
{   //Read a gain register. Not valid for temperature gain
    //Just call get_i_v_p_q_measurement, it does the same thing
    uint8_t data[3] = { 0 };
    int err = this->read_register(type, data);
    uint32_t reg_val = three_bytes_to_uint(data);
    *result = reg_val / float(1 << 22);
    return err;
}

int CS5463::set_gain(registers type, float gain)
{   //Not valid for temperature gain
    uint8_t data[3] = { 0 };
    uint_to_three_bytes(gain * (1 << 24), data);
    return this->write_register(type, data);
}


int CS5463::get_i_v_p_q_measurement(registers type, float *result)
{   //Get measurement from all the peak and instantanious registers
    //Basicly all registers with measurements, except i_rms and v_rms
    uint8_t data[3] = { 0 };
    int err = this->read_register(type, data);
    int32_t reg_val = three_bytes_to_int(data);

    *result = reg_val / float(1 << 23);
    return err;
}

int CS5463::get_rms_measurement(registers type, float *result)
{   //Get measurement from RMS registers.
    uint8_t data[3] = { 0 };
    int err = this->read_register(type, data);
    int32_t reg_val = three_bytes_to_int(data);
    *result = reg_val / float(1 << 24);
    return err;
}
