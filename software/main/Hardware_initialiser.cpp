#include "Hardware_Initialiser.h"
#include "HardwareVersion.h"
#include "PCF8574_Handler.h"
#include "SwitchHandler.h"
#include "EnergyMonitor.h"
int Hardware_Initialiser::initialise_hardware()
{
    return Hardware_Initialiser::initialise_hardware_1_1();
    HardwareVersion::read_hardware_version();
    hardware_version_t hardware_version = HardwareVersion::get_hardware_version();
    printf("Initialising hardware for PCB version %d.%d\n", hardware_version.gpio_version, hardware_version.external_version);
    switch(hardware_version.gpio_version)
    {
        case 1:
            return Hardware_Initialiser::initialise_hardware_1(hardware_version.external_version);
        default:
            printf("Panic, not a known hardware version (gpio). Am I a potato?\n");
            return 1;
    }
}

int Hardware_Initialiser::initialise_hardware_1(uint8_t external_version)
{
    switch(external_version)
    {
        case 1:
            return Hardware_Initialiser::initialise_hardware_1_1();
        default:
            printf("Panic, not a known hardware version (external). Am I a potato?\n");
            return 1;

    }
}

int Hardware_Initialiser::initialise_hardware_1_1()
{
    printf("Valid PCB version found, now initialising hardware v1.1\n");
    printf("Initialising PCF8574 devices\n");
    static PCF8574 PCF8574_devices[2];
    PCF8574_devices[0].address = 0x21;
    PCF8574_devices[0].interrupt_pin = (gpio_num_t)0;
    PCF8574_devices[1].address = 0x20;
    PCF8574_devices[1].interrupt_pin = GPIO_NUM_23;
    PCF8574_Handler::get_instance(GPIO_NUM_21, GPIO_NUM_19, PCF8574_devices, 2);

    printf("Intialising switch handler!\n");
    static const gpio_num_t button_leds[] = {
        GPIO_NUM_27,
        GPIO_NUM_26,
        GPIO_NUM_32,
    };
    static PCF8574_pin relay_pins[3];
    static PCF8574_pin button_pins[3];
    static PCF8574_pin relay_voltage_pin;
    relay_voltage_pin.address = 0x20;
    relay_voltage_pin.pin_num = 0;
    relay_pins[0].address = 0x20;
    relay_pins[1].address = 0x20;
    relay_pins[2].address = 0x20;
    relay_pins[0].pin_num = 3;
    relay_pins[1].pin_num = 2;
    relay_pins[2].pin_num = 1;

    button_pins[0].address = 0x21;
    button_pins[1].address = 0x21;
    button_pins[2].address = 0x21;
    button_pins[0].pin_num = 0;
    button_pins[1].pin_num = 1;
    button_pins[2].pin_num = 2;
    SwitchHandler::get_instance(relay_pins, &relay_voltage_pin, button_pins, button_leds, 3);
    static gpio_num_t cs5463_slave_selects[] = {
    //    GPIO_NUM_17,
    //    GPIO_NUM_4,
        GPIO_NUM_16,
    };
    static PCF8574_pin reset_pin;
    reset_pin.address = 0x20;
    reset_pin.pin_num = 5;

    printf("Initialisin energy monitoring\n");
    CS5463::init_spi(GPIO_NUM_22, GPIO_NUM_18, GPIO_NUM_5);
    EnergyMonitor::get_instance(cs5463_slave_selects, &reset_pin, 1);
    return 0;
}
