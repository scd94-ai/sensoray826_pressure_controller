#ifndef SENSORAY826_H
#define SENSORAY826_H

class Sensoray826 {
private:
    unsigned int board_num_;
    bool system_open_;
    bool adc_running_;

public:
    explicit Sensoray826(unsigned int board_num = 0);
    ~Sensoray826();

    int initialize();
    int close();

    int configureDac(unsigned int dac_channel);
    int writeDac(unsigned int dac_channel, unsigned int setpoint);

    int configureAdcSlot(unsigned int adc_slot, unsigned int adc_channel);
    int startAdc();
    int stopAdc();

    int discardAdcSample(unsigned int adc_slot);
    int readAdcVoltage(
        unsigned int adc_slot,
        double& voltage,
        unsigned int timeout_ms = 1000
    );

    unsigned int boardNumber() const;
    bool isOpen() const;
};

#endif
