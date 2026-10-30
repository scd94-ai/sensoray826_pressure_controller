
#include "Potentiometer.h"
#include "826api.h"

#include <iostream>
#include <limits>
#include <algorithm>
#include <cmath>

// Constructor
Potentiometer::Potentiometer(
    Sensoray826& sensoray,
    unsigned int adc_channel,
    unsigned int adc_slot,
    double min_voltage,
    double max_voltage
)
    : Sensoray826_Device(sensoray),
      adc_channel_(adc_channel),
      adc_slot_(adc_slot),
      min_voltage_(min_voltage),
      max_voltage_(max_voltage)
{
}

// Initialize potentiometer
int Potentiometer::initialize()
{
    const int status = configureADC(adc_slot_, adc_channel_);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to configure potentiometer ADC slot "
                  << adc_slot_ << ": " << status << '\n';
        return status;
    }

    std::cout << "Potentiometer initialized."
              << " ADC=" << adc_channel_
              << " slot=" << adc_slot_ << '\n';

    return S826_ERR_OK;
}

// Read voltage
int Potentiometer::readVoltage(double& voltage)
{
    const int status =
        Sensoray826_Device::readVoltage(adc_slot_, voltage);

    if (status != S826_ERR_OK) {
        std::cerr << "Potentiometer ADC read failed: "
                  << status << '\n';
    }

    return status;
}

// Convert voltage to percentage
double Potentiometer::readPercent(double voltage) const
{
    if (!std::isfinite(voltage) ||
        !std::isfinite(min_voltage_) ||
        !std::isfinite(max_voltage_) ||
        max_voltage_ <= min_voltage_) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return std::clamp(
        (voltage - min_voltage_) /
        (max_voltage_ - min_voltage_) * 100.0,
        0.0,
        100.0
    );
}
