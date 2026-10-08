#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

class Potentiometer{
    private:
        unsigned int board_num_;
        unsigned int adc_channel_;
        unsigned int adc_slot_;

    double min_voltage_;
    double max_voltage_;

    int initializeADC();
    public:
        Potentiometer(
        unsigned int board_num = 0,
        unsigned int adc_channel = 12,
        unsigned int adc_slot = 4,
        double min_voltage = 0.0,
        double max_voltage = 5.0
        );

        int initialize();

        int readVoltage(double& voltage);

        double readPercent(double voltage);
};

#endif