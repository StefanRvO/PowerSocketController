#include "HardwareVersion.h"
#define GPIO_VERSION_PIN GPIO_NUM_34
#define I2C_SCL GPIO_NUM_21
#define I2C_SDA GPIO_NUM_19
#include "i2cscanner.h"

hardware_version_t HardwareVersion::hardware_version;

void HardwareVersion::read_hardware_version()
{

    HardwareVersion::hardware_version.gpio_version = HardwareVersion::read_gpio_version();
    printf("gpio hardware version was %d!\n", HardwareVersion::hardware_version.gpio_version);
    HardwareVersion::hardware_version.external_version = HardwareVersion::read_external_version();
    if(HardwareVersion::hardware_version.external_version == 0)
    {
        printf("Panic, external hardware verison invalid, was %d!\n", HardwareVersion::hardware_version.external_version);
        HardwareVersion::hardware_version.gpio_version = 0;
    }
}

hardware_version_t HardwareVersion::get_hardware_version()
{
    return HardwareVersion::hardware_version;
}


uint8_t HardwareVersion::read_external_version()
{
    if(HardwareVersion::hardware_version.gpio_version != 1) //This is the only gpio version for now
    {
        printf("Panic, GPIO hardware verison invalid, was %d!\n", HardwareVersion::hardware_version.gpio_version);
        return 0;
    }
    uint8_t version = 0;
    uint8_t *i2c_devices = nullptr;
    uint8_t i2c_count = 0;
    printf("Detecting external hardware version based on i2c devices\n");
    i2cscanner(I2C_SCL, I2C_SDA, &i2c_devices, &i2c_count);
    printf("Detected %d i2c devices:\n", i2c_count);
    for(uint8_t i = 0; i < i2c_count; i++)
        printf("I2C: 0x%.2x detected\n", i2c_devices[i]);
    if(i2c_count == 2 && i2c_devices[0] >= 0x20) //PCF8574 have addresses 0x20 - 0x27
    {
        version += i2c_devices[0] - 0x20;
        version += 1;
        for(uint8_t i = 0; i < i2c_devices[1] - 0x21; i++)
        {
            version += i;
        }
    }
    return version;
}

uint8_t HardwareVersion::read_gpio_version()
{ //Get the portion of the version which is determined by GPIO pins.
    printf("Detecting gpio hardware verision\n");
    return HardwareVersion::read_pin_state(GPIO_VERSION_PIN) + 1;
}

pin_state HardwareVersion::read_pin_state(gpio_num_t pin)
{ //Measurers gpio state with a pullup and a pulldown.
  //Leave pin in a state with no pullup/down.
    bool high_with_pullup;
    bool high_with_pulldown;
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = ((uint64_t)1) << (uint64_t)pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = (gpio_pulldown_t) 0;
    io_conf.pull_up_en = (gpio_pullup_t)1;

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    high_with_pullup = gpio_get_level(pin);
    gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
    high_with_pulldown = gpio_get_level(pin);
    gpio_set_pull_mode(pin, GPIO_FLOATING);
    if(high_with_pulldown && high_with_pullup)
        return pin_state::floating;
    else if(high_with_pullup)
        return pin_state::HIGH;
    else
        return pin_state::LOW;
}
