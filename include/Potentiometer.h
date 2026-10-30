
#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include "Sensoray826_Device.h"

class Potentiometer : public Sensoray826_Device {

private:
    unsigned int adc_channel_;
    unsigned int adc_slot_;

    double min_voltage_;
    double max_voltage_;

public:
    Potentiometer(
        Sensoray826& sensoray,
        unsigned int adc_channel,
        unsigned int adc_slot,
        double min_voltage = 0.0,
        double max_voltage = 5.0
    );

    int initialize() override;

    int readVoltage(double& voltage);

    double readPercent(double voltage) const;
};

#endif
