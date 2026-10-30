
#include "Pressure_Sensor.h"
#include "826api.h"

#include <iostream>

// Constructor
Pressure_Sensor::Pressure_Sensor(
    Sensoray826& sensoray,
    unsigned int adc_channel,
    unsigned int adc_slot
)
    : Sensoray826_Device(sensoray),
      adc_channel_(adc_channel),
      adc_slot_(adc_slot)
{
}

// Initialize pressure sensor
int Pressure_Sensor::initialize()
{
    const int status = configureADC(adc_slot_, adc_channel_);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to configure pressure sensor ADC slot "
                  << adc_slot_ << ": " << status << '\n';
        return status;
    }

    std::cout << "Pressure sensor initialized."
              << " ADC=" << adc_channel_
              << " slot=" << adc_slot_ << '\n';

    return S826_ERR_OK;
}

// Read voltage
int Pressure_Sensor::readVoltage(double& voltage)
{
    const int status =
        Sensoray826_Device::readVoltage(adc_slot_, voltage);

    if (status != S826_ERR_OK) {
        std::cerr << "Pressure sensor ADC read failed: "
                  << status << '\n';
    }

    return status;
}

// Convert voltage to pressure
bool Pressure_Sensor::readPressure(double& pressure_kpa)
{
    double voltage = 0.0;

    if (readVoltage(voltage) != S826_ERR_OK) {
        return false;
    }

    pressure_kpa = (voltage - 2.5) * 50.0;

    return true;
}
