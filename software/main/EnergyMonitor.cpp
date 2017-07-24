#include "EnergyMonitor.h"
extern "C"
{
    #include <stdio.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_system.h"
    #include "esp_log.h"
}
#include <cstring>
using namespace PCF8574_enum;

EnergyMonitor *EnergyMonitor::instance;

EnergyMonitor *EnergyMonitor::get_instance()
{
    return EnergyMonitor::instance;
}

EnergyMonitor *EnergyMonitor::get_instance(gpio_num_t *slave_selects, PCF8574_pin *reset_pin, uint8_t _device_count)
{
    if(EnergyMonitor::instance == nullptr)
    {
        EnergyMonitor::instance = new EnergyMonitor(slave_selects, reset_pin, _device_count);
    }
    return EnergyMonitor::instance;
}

EnergyMonitor::EnergyMonitor(gpio_num_t *slave_selects, PCF8574_pin *_reset_pin, uint8_t _device_count)
: pcf8574(PCF8574_Handler::get_instance()), reset_pin(_reset_pin)
{
    devices.reserve(_device_count);
    for(uint8_t i = 0; i < _device_count; i++)
        devices.push_back(CS5463(slave_selects[i]));

    PCF8574_pin_state reset_state = HIGH;
    this->pcf8574->set_as_output(_reset_pin, &reset_state, 1);

    xTaskCreate(EnergyMonitor::energy_monitor_thread_wrapper, "energy_thread", 4096, (void **)this, 10, NULL);
}

void EnergyMonitor::energy_monitor_thread_wrapper(void *user)
{
    EnergyMonitor::instance->energy_monitor_thread();
}

void EnergyMonitor::energy_monitor_thread()
{
    this->cs5463_setup();
    uint32_t i = 0;

    while(true)
    {
        if(i < 400 && ++i == 400)
            for(auto &device : this->devices)
            {   //Start frequency measurement 20 seconds after boot
                device.set_frequency_measurement(true);
            }

        for(uint8_t j = 0; j < this->devices.size(); j++)
        {
            status_register reg;
            this->devices[j].read_status_register(&reg);
            if(reg.data_ready)
                this->handle_drdy(j);
            if(reg.conversion_ready)
                this->handle_crdy(j);
            if(reg.temp_update)
                this->handle_tup(j);
            if(reg.epsilon_update)
                this->handle_fup(j);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void EnergyMonitor::cs5463_setup()
{   //Sets up the CS5463
    //Perform a hardware reset:
    this->pcf8574->set_output_state(*reset_pin, LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    this->pcf8574->set_output_state(*reset_pin, HIGH);
    vTaskDelay(60 / portTICK_PERIOD_MS);
    //Start energy monitoring
    for(auto &device : this->devices)
    {
        device.start_conversions();

        device.set_computation_cycle_duration(250); //250 ms per cycle
        ctrl_register reg;
        reg.stop = 0;
        reg.int_opendrain = 0;
        reg.no_cpu = 1;
        reg.no_osc = 0;
        device.set_control(reg);
        float e = 50. / device.owr;
        device.set_epsilon(e);
    }

}

void EnergyMonitor::handle_drdy(uint8_t device)
{
    //printf("Device %d have DRDY\n", device);
    status_register reg;
    memset(&reg, 0, sizeof(reg));
    reg.data_ready = 1;
    this->devices[device].set_status_register(reg);
}

void EnergyMonitor::handle_crdy(uint8_t device)
{
    //printf("Device %d have CRDY\n", device);
    status_register reg;
    memset(&reg, 0, sizeof(reg));
    reg.conversion_ready = 1;
    this->devices[device].set_status_register(reg);
}

void EnergyMonitor::handle_tup(uint8_t device)
{
    //printf("Device %d have TUP\n", device);
    float temp;
    this->devices[device].read_temperature(&temp);
    printf("temp, device %d: %f\n", device, temp);
    status_register reg;
    memset(&reg, 0, sizeof(reg));
    reg.temp_update = 1;
    this->devices[device].set_status_register(reg);
}

void EnergyMonitor::handle_fup(uint8_t device)
{
    //printf("Device %d have FUP\n", device);
    status_register reg;
    memset(&reg, 0, sizeof(reg));
    reg.epsilon_update = 1;
    this->devices[device].set_status_register(reg);
}

void EnergyMonitor::print_status_register()
{
    for(uint8_t i = 0; i < this->devices.size(); i++)
    {
        status_register status_reg;
        printf("------------ Status Register, device %d ------------\n", i);
        ESP_ERROR_CHECK(devices[i].read_status_register(&status_reg));
        printf("DRDY: %d\nCRDY: %d\n", status_reg.data_ready, status_reg.conversion_ready);
        printf("IOR: %d\nVOR: %d\n", status_reg.current_out_range, status_reg.voltage_out_range);
        printf("IROR: %d\nVROR: %d\n", status_reg.irms_out_range, status_reg.vrms_out_range);
        printf("EOR: %d\nIFAULT: %d\n", status_reg.energy_out_range, status_reg.current_fault);
        printf("VSAG: %d\nTUP: %d\n", status_reg.voltage_sag, status_reg.temp_update);
        printf("TOD: %d\nVOD: %d\n", status_reg.temp_modu_oscilation, status_reg.voltage_modu_oscilation);
        printf("IOD: %d\nLSD: %d\n", status_reg.current_modu_oscilation, status_reg.low_supply_detected);
        printf("FUP: %d\nIC: %d\n", status_reg.epsilon_update, status_reg.invalid_command);

    }

}
