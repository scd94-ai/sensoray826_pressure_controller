
#include "Sensoray826_Device.h"
#include "826api.h"

#include <cmath>

// Constructor
Sensoray826_Device::Sensoray826_Device(Sensoray826& sensoray)
    : sensoray_(sensoray)
{
}

// Configure ADC
int Sensoray826_Device::configureADC(
    unsigned int adc_slot,
    unsigned int adc_channel)
{
    return sensoray_.configureAdcSlot(adc_slot, adc_channel);
}

// Read ADC voltage
int Sensoray826_Device::readVoltage(
    unsigned int adc_slot,
    double& voltage,
    unsigned int timeout_ms)
{
    return sensoray_.readAdcVoltage(
        adc_slot,
        voltage,
        timeout_ms
    );
}

// Configure DAC
int Sensoray826_Device::configureDAC(unsigned int dac_channel)
{
    return sensoray_.configureDac(dac_channel);
}

// Write DAC voltage
int Sensoray826_Device::writeVoltage(
    unsigned int dac_channel,
    double voltage)
{
    if (!std::isfinite(voltage) ||
        voltage < 0.0 || voltage > 10.0) {
        return -1;
    }

    unsigned int setpoint = static_cast<unsigned int>(
        std::lround(voltage * 65535.0 / 10.0)
    );

    return sensoray_.writeDac(dac_channel, setpoint);
}
