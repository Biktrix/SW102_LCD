#include <inttypes.h>

uint16_t emu_voltage = 50;

uint16_t battery_voltage_10x_get()
{
    return emu_voltage;
}
