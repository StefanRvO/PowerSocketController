#include "PCF8574_Handler.h"
#include "driver/i2c.h"
#include <cstring>
#define ENABLE_I2C_PULLUP GPIO_PULLUP_ENABLE
#define I2C_CLK_FREQ 100000
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TX_BUF_DISABLE 0
#define ACK_VAL 0x0

static int setup_i2c_master(gpio_num_t scl, gpio_num_t sda)
{
    i2c_port_t i2c_master_port = I2C_NUM_0;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = ENABLE_I2C_PULLUP;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = ENABLE_I2C_PULLUP;
    conf.master.clk_speed = I2C_CLK_FREQ;
    ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0));
    return 0;
}

PCF8574_Handler *PCF8574_Handler::instance = nullptr;
PCF8574_State *PCF8574_Handler::devices = nullptr;
uint8_t PCF8574_Handler::device_count = 0;
std::list<PCF8574InterruptNotice> PCF8574_Handler::interrupt_notices;
PCF8574_Handler *PCF8574_Handler::get_instance()
{
    return PCF8574_Handler::instance;
}

PCF8574_Handler *PCF8574_Handler::get_instance(gpio_num_t scl, gpio_num_t sda, PCF8574 *devices, uint8_t device_count)
{
    if(PCF8574_Handler::instance == nullptr)
    {
        PCF8574_Handler::instance = new PCF8574_Handler(scl, sda, devices, device_count);
    }

    return PCF8574_Handler::instance;
}

PCF8574_State *PCF8574_Handler::find_device(uint8_t address)
{
    for(uint8_t i = 0; i < this->device_count; i++)
        if (this->devices[i].device.address == address)
            return this->devices + i;
    return nullptr;
}

int PCF8574_Handler::register_interrupt_notice(uint8_t address, void (*callback)(uint8_t, void *), void *user_data)
{
    PCF8574InterruptNotice notice;
    notice.address = address;
    notice.user_data = user_data;
    notice.callback = callback;
    this->interrupt_notices.push_back(notice);
    return 0;
}

PCF8574_Handler::PCF8574_Handler(gpio_num_t scl, gpio_num_t sda, PCF8574 *_devices, uint8_t _device_count)
{

    setup_i2c_master(scl, sda);
    this->devices = new PCF8574_State[_device_count];
    this->device_count = _device_count;
    for(uint8_t i = 0; i < _device_count; i++)
    {
        memcpy(&this->devices[i].device, devices + i, sizeof(this->devices[0].device));
        //Setup interrupts
        gpio_num_t &interrupt_pin = this->devices[i].device.interrupt_pin;
        if(interrupt_pin == 0) continue;
        gpio_config_t io_conf;
        //interrupt of falling edge
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.pin_bit_mask = interrupt_pin;
        //set as input mode
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = (gpio_pulldown_t) 0;
        //enable pull-up mode
        io_conf.pull_up_en = (gpio_pullup_t)1;
        //configure GPIO with the given settings
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        //install gpio isr service
        ESP_ERROR_CHECK(gpio_install_isr_service(0)); //Are we allowed to do this multiple times?
        //hook isr handler for specific gpio pin
        ESP_ERROR_CHECK(gpio_isr_handler_add(interrupt_pin, PCF8574_Handler::interrupt_handler, (void*) &this->devices[i].device));
    }
}

void PCF8574_Handler::interrupt_handler(void *arg)
{
    PCF8574 *device = (PCF8574 *)arg;
    for(auto &notice : PCF8574_Handler::interrupt_notices)
    {
        if(notice.address == device->address)
        {
            notice.callback(notice.address, notice.user_data);
        }
    }
}

int PCF8574_Handler::set_as_output(const PCF8574_pin *pins, const PCF8574_pin_state* states, uint8_t pin_count)
{
    if(pin_count == 0) return 0;
    PCF8574_State *device = this->find_device(pins->address);
    for(uint8_t i = 0; i < pin_count; i++)
    {
        device->pin_types &= ~(1 << pins[i].pin_num);
        device->output_state |= (states[i] << pins[i].pin_num);
    }
    this->write_state(device);
    return 0;
}

int PCF8574_Handler::set_as_input(const PCF8574_pin *pins, uint8_t pin_count)
{
    if(pin_count == 0) return 0;
    PCF8574_State *device = this->find_device(pins->address);
    for(uint8_t i = 0; i < pin_count; i++)
        device->pin_types |= (1 << pins[i].pin_num);
    this->write_state(device);
    return 0;
}

int PCF8574_Handler::set_output_state(const PCF8574_pin *pins, const PCF8574_pin_state* states, uint8_t pin_count)
{
    if(pin_count == 0) return 0;
    PCF8574_State *device = this->find_device(pins->address);
    for(uint8_t i = 0; i < pin_count; i++)
        device->output_state |= (states[i] << pins[i].pin_num);
    this->write_state(device);
    return 0;
}

int PCF8574_Handler::read_input_state(const PCF8574_pin *pins, PCF8574_pin_state* states, uint8_t pin_count)
{
    if(pin_count == 0) return 0;
    PCF8574_State *device = this->find_device(pins->address);
    uint8_t state = 0;
    this->read_state(device, &state);
    for(uint8_t i = 0; i < pin_count; i++)
    {
        states[i] = (PCF8574_pin_state) ((state >> pins[i].pin_num) & 1);
    }
    return 0;
}

int PCF8574_Handler::read_state(PCF8574_State *device, uint8_t *state)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    //Send address and read command
    i2c_master_write_byte(cmd, (device->device.address << 1) | I2C_MASTER_READ, ACK_VAL);
    //Read the response byte
    i2c_master_read_byte(cmd, state, ACK_VAL);
    i2c_master_stop(cmd);
    xSemaphoreTake(this->i2c_mux, portMAX_DELAY);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 20 / portTICK_RATE_MS); //20 ms to perform this
    xSemaphoreGive(this->i2c_mux);
    i2c_cmd_link_delete(cmd);
    return ret;
}

int PCF8574_Handler::write_state(PCF8574_State *device)
{
    uint8_t data = device->output_state | device->pin_types;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    //Send address and read command
    i2c_master_write_byte(cmd, (device->device.address << 1) | I2C_MASTER_WRITE, ACK_VAL);
    i2c_master_write_byte(cmd, data, ACK_VAL);
    i2c_master_stop(cmd);
    xSemaphoreTake(this->i2c_mux, portMAX_DELAY);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 20 / portTICK_RATE_MS); //20 ms to perform this
    xSemaphoreGive(this->i2c_mux);
    i2c_cmd_link_delete(cmd);
    return ret;
}
