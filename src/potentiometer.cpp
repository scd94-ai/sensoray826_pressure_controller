#include "Potentiometer.h"
#include "826api.h"

#include <iostream>

Potentiometer::Potentiometer(
    Sensoray826& sensoray,
    unsigned int adc_channel,
    unsigned int adc_slot,
    double min_voltage,
    double max_voltage
)
    : sensoray_(sensoray),
      adc_channel_(adc_channel),
      adc_slot_(adc_slot),
      min_voltage_(min_voltage),
      max_voltage_(max_voltage)
{
}

int Potentiometer::initialize()
{
    const int status =
        sensoray_.configureAdcSlot(
            adc_slot_,
            adc_channel_
        );

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

int Potentiometer::readVoltage(double& voltage)
{
    const int status =
        sensoray_.readAdcVoltage(
            adc_slot_,
            voltage,
            1000
        );

    if (status != S826_ERR_OK) {
        std::cerr << "Potentiometer ADC read failed: "
                  << status << '\n';
    }

    return status;
}

double Potentiometer::readPercent(double voltage) const
{
    return (voltage - min_voltage_) /
           (max_voltage_ - min_voltage_) * 100.0;
}
