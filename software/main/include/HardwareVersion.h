#pragma once
//Class to hold and read information about which hardware version the software is currently running on
#include <cstdint>

class HardwareVersion
{
    public:
        static uint8_t hardware_version;
        static uint8_t get_hardware_version();
        static void read_hardware_version();
    private:
};
