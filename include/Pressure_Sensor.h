#ifndef PRESSURE_SENSOR_H
#define PRESSURE_SENSOR_H

#include "Sensoray826.h"


class Pressure_Sensor {
    private:
        Sensoray826& sensoray_;
    
        unsigned int adc_channel_;
        unsigned int adc_slot_;
    public:
            Pressure_Sensor(
            Sensoray826& sensoray,
            unsigned int adc_channel,
            unsigned int adc_slot
        );

        int initialize();
        double readVoltage(double& voltage);
        bool readPressure(double& pressure_kpa);

};

#endif