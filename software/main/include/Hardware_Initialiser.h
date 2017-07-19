#pragma once
#include <cstdint>
class Hardware_Initialiser
{
    public:
        static int initialise_hardware();
    private:
        static int initialise_hardware_1(uint8_t external_version);
        static int initialise_hardware_1_1();
};
