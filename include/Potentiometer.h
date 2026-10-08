#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include "Sensoray826.h"

class Potentiometer {
private:
    Sensoray826& sensoray_;

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

    int initialize();
    int readVoltage(double& voltage);
    double readPercent(double voltage) const;
};

#endif
