#ifndef SENSORAY826_H
#define SENSORAY826_H

#include "BoardOwnership.h"
#include <array>
#include <cstdint>
#include <string>

struct PotentiometerSample {
    std::array<double, 2> voltage_v{};
    std::array<std::uint32_t, 2> timestamps_us{};
    std::array<bool, 2> valid{};
    int sdk_status = 0;
    bool supply_valid = false;
    double supply_voltage_v = 0.0;
    std::uint32_t supply_timestamp_us = 0;
    bool have_new_data = false;
};

class Sensoray826 {
private:
    unsigned int board_num_;
    bool system_open_;
    bool adc_running_;
    std::string initialization_error_;
    BoardOwnership ownership_;
    bool snapshot_valid_ = false;
    unsigned int original_slotlist_ = 0, original_trigger_ = 0;
    std::array<std::array<unsigned int, 3>, 16> original_slots_{};
    unsigned int touched_slots_ = 0;
    bool adc_control_touched_ = false;
    bool potentiometers_ = false, include_supply_ = false;
    unsigned int potentiometer_mask_ = 0;
    std::array<std::uint32_t, 3> previous_timestamp_{};
    std::array<bool, 3> have_timestamp_{}, timestamp_fault_{};
    int verifyAdc(unsigned int enabled, unsigned int slots, unsigned int trigger);

public:
    explicit Sensoray826(unsigned int board_num = 0);
    ~Sensoray826();
    Sensoray826(const Sensoray826&) = delete;
    Sensoray826& operator=(const Sensoray826&) = delete;

    int initialize();
    std::string initializationError() const;
    int close();

    int configureDac(unsigned int dac_channel);
    int writeDac(unsigned int dac_channel, unsigned int setpoint);

    int configureAdcSlot(unsigned int adc_slot, unsigned int adc_channel);
    int startAdc();
    int stopAdc();

    // Sole ADC owner: one zero-timeout batch read, never legacy 1000 ms reads.
    int startPotentiometers(bool include_supply = false, unsigned int settling_us = 100);
    PotentiometerSample readPotentiometers();
    const std::string& ownershipLimitations() const { return ownership_.limitations(); }

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
