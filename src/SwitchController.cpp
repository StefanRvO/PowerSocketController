#include "SwitchController.h"
#include "SettingsReader.h"
extern "C"
{
    #include "espressif/esp_common.h"
    #include "esp8266.h"

}

#include <cstring>

SwitchController *SwitchController::controller = nullptr;

SwitchController *SwitchController::get_instance(uint8_t _switch_count, uint8_t *_switch_list)
{
    if(SwitchController::controller == nullptr)
    {
        SwitchController::controller = new SwitchController(_switch_count, _switch_list);
    }
    return SwitchController::controller;
}

SwitchController *SwitchController::get_instance()
{
    return SwitchController::controller;
}

SwitchController::SwitchController(uint8_t _switch_count, uint8_t *_switch_list)
{
    this->switch_count = _switch_count;
    this->switch_list = new uint8_t[_switch_count];
    memcpy(this->switch_list, _switch_list, _switch_count);
    for (uint8_t i = 0; i < this->switch_count; i++)
    {
        gpio_enable(this->switch_list[i], GPIO_OUTPUT);
    }
}

bool SwitchController::destroy_instance()
{
    if(SwitchController::controller != nullptr)
    {
        delete SwitchController::controller;
        return true;
    }
    else return false;
}

SwitchController::~SwitchController()
{
    for (uint8_t i = 0; i < this->switch_count; i++)
    {
        gpio_disable(this->switch_list[i]);
    }
    delete[] this->switch_list;
    SwitchController::controller = nullptr;

}

void SwitchController::loadstate()
{
    for(uint8_t i = 0; i < this->switch_count; i++)
    {
        gpio_write(this->switch_list[i], SettingsReader::get_switch_state(i));
    }
}

bool SwitchController::set_switch_pins(uint8_t _switch_count, uint8_t *_switch_list)
{
    for (uint8_t i = 0; i < this->switch_count; i++)
    {
        gpio_disable(this->switch_list[i]);
    }
    delete[] this->switch_list;
    this->switch_count = switch_count;
    this->switch_list = new uint8_t[_switch_count];
    memcpy(this->switch_list, _switch_list, _switch_count);
    for (uint8_t i = 0; i < this->switch_count; i++)
    {
        gpio_enable(this->switch_list[i], GPIO_OUTPUT);
    }
}

bool SwitchController::change_switch(uint8_t pin, bool state)
{
    if(SwitchController::controller == nullptr) return false;
    if(this->switch_count <= pin) return false;
    gpio_write(this->switch_list[pin], state);
    SettingsReader::set_switch_state(pin, state);
    return true;
}

bool SwitchController::read_switch(uint8_t pin)
{
    if(SwitchController::controller == nullptr) return false;
    if(this->switch_count <= pin) return false;
    return gpio_read(this->switch_list[pin]);
}
