#ifndef FESTO_H
#define FESTO_H


class Festo {

private:
    // --------------------
    // Member variables
    // --------------------
    unsigned int board_num_;
    unsigned int dac_channel_;
    unsigned int adc_channel_;
    unsigned int adc_slot_;

    double min_pressure_bar_;
    double max_pressure_bar_;

    double min_voltage_;
    double max_voltage_;

    // --------------------
    // Private helper functions
    // --------------------

    double pressureToVoltage(double pressure_bar);

    double voltageToPressure(double voltage);

    unsigned int pressureToDac(double pressure_bar);

    int initializeADC();

    int generateSineWave(double* buf, int resolution);


public:
    // --------------------
    // Constructor
    // --------------------

    Festo(
        unsigned int board_num = 0,
        unsigned int dac_channel = 0,
        unsigned int adc_channel = 15,
        unsigned int adc_slot = 0,
        double min_pressure_bar = -1.0,
        double max_pressure_bar = 1.0,
        double min_voltage = 0.0,
        double max_voltage = 10.0
    );


    // --------------------
    // Public functions
    // --------------------

    int initialize();

    int setPressure(double pressure_bar);

    int readPressure(
        double& voltage,
        double& pressure_bar,
        unsigned int response_wait_ms = 100
    );

    int runDiagnostic();
};

#endif