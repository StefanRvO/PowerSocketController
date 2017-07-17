#include "HardwareVersion.h"

uint8_t HardwareVersion::hardware_version;

void HardwareVersion::read_hardware_version()
{
    HardwareVersion::hardware_version = 0; //We only have one version for now.
}

uint8_t HardwareVersion::get_hardware_version()
{
    return HardwareVersion::hardware_version;
}
