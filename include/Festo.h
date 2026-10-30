
#ifndef FESTO_H
#define FESTO_H

#include "Sensoray826_Device.h"

class Festo : public Sensoray826_Device {

private:
    unsigned int dac_channel_;
    unsigned int adc_channel_;
    unsigned int adc_slot_;

    double min_pressure_kpa_;
    double max_pressure_kpa_;

    double min_voltage_;
    double max_voltage_;

    double pressureToVoltage(double pressure_kpa) const;
    double voltageToPressure(double voltage) const;

    int generateSineWave(double* buf, int resolution) const;

public:
    Festo(
        Sensoray826& sensoray,
        unsigned int dac_channel,
        unsigned int adc_channel,
        unsigned int adc_slot,
        double min_pressure_kpa = -50.0,
        double max_pressure_kpa = 50.0,
        double min_voltage = 0.0,
        double max_voltage = 5.0
    );

    int initialize() override;

    int setPressure(double pressure_kpa);
    int setPressure(double pressure_kpa, bool log_success);
    
    int readPressure(
        double& voltage,
        double& pressure_kpa,
        unsigned int response_wait_ms = 100
    );

    int runDiagnostic();
};

#endif
