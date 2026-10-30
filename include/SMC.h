
#ifndef SMC_H
#define SMC_H

#include "Sensoray826_Device.h"

class SMC : public Sensoray826_Device {

private:
    unsigned int dac_channel_;

    double pressureToVoltage(double pressure_kpa) const;

public:
    explicit SMC(
        Sensoray826& sensoray,
        unsigned int dac_channel = 3
    );

    int initialize() override;

    int setPressure(double pressure_kpa);

    // Command minimum vacuum (0 V)
    int reset();
};

#endif
