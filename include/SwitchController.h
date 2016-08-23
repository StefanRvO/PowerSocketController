#pragma once
#include <cstdint>
class SwitchController;

class SwitchController
{
    private:
        //Constructs the switch controller
        //The given pins will be the ones which will be used by the switch controller.
        //The switch controller will be a singleton. Once created, the switch list
        //can only be changed by calling set_switch_pins
        static SwitchController *controller;
        SwitchController(uint8_t switch_count, uint8_t *switch_list);
        uint8_t switch_count = 0;
        uint8_t *switch_list = nullptr;
        ~SwitchController();
    public:
        static SwitchController *get_instance(uint8_t switch_count, uint8_t *switch_list);
        static SwitchController *get_instance();

        bool destroy_instance();
        bool set_switch_pins(uint8_t switch_count, uint8_t *switch_list);
        bool change_switch(uint8_t pin, bool state);
        bool read_switch(uint8_t pin);
        void loadstate();
};
