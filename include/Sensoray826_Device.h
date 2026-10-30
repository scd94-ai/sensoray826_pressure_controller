
#ifndef SENSORAY826_DEVICE_H
#define SENSORAY826_DEVICE_H

#include "Sensoray826.h"

class Sensoray826_Device {

protected:
    Sensoray826& sensoray_;

    // ADC operations
    int configureADC(unsigned int adc_slot, unsigned int adc_channel);

    int readVoltage(
        unsigned int adc_slot,
        double& voltage,
        unsigned int timeout_ms = 1000
    );

    // DAC operations
    int configureDAC(unsigned int dac_channel);

    int writeVoltage(
        unsigned int dac_channel,
        double voltage
    );

public:
    explicit Sensoray826_Device(Sensoray826& sensoray);

    virtual ~Sensoray826_Device() = default;

    virtual int initialize() = 0;
};

#endif
