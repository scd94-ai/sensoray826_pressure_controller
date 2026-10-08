#include "Potentiometer.h"
#include "826api.h"

#include <iostream>

namespace {
    constexpr unsigned int ADC_SETTLING_TIME_US = 0;
}

Potentiometer::Potentiometer(
    unsigned int board_num,
    unsigned int adc_channel,
    unsigned int adc_slot,
    double min_voltage,
    double max_voltage
)
    : board_num_(board_num),
      adc_channel_(adc_channel),
      adc_slot_(adc_slot),
      min_voltage_(min_voltage),
      max_voltage_(max_voltage)
{
}

int Potentiometer::initializeADC()
{
    int status = S826_AdcSlotConfigWrite(
        board_num_,
        adc_slot_,
        adc_channel_,
        ADC_SETTLING_TIME_US,
        S826_ADC_GAIN_1
    );

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to configure potentiometer ADC slot: "
                  << status << '\n';
        return status;
    }

    status = S826_AdcSlotlistWrite(
        board_num_,
        (1u << adc_slot_),
        S826_BITSET
    );

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to enable potentiometer ADC slot: "
                  << status << '\n';
        return status;
    }

    return S826_ERR_OK;
}

int Potentiometer::initialize()
{
    int status = initializeADC();

    if (status != S826_ERR_OK) {
        return status;
    }

    std::cout << "Potentiometer initialized.\n"
              << "ADC channel: " << adc_channel_ << '\n'
              << "ADC slot: " << adc_slot_ << '\n';

    return S826_ERR_OK;
}

int Potentiometer::readVoltage(double& voltage)
{
    int buf[16] = {};
    uint slotlist = (1u << adc_slot_);

    int status = S826_AdcRead(
        board_num_,
        buf,
        nullptr,
        &slotlist,
        1000
    );

    if (status != S826_ERR_OK &&
        status != S826_ERR_MISSEDTRIG) {
        std::cerr << "Potentiometer ADC read failed: "
                  << status << '\n';
        return status;
    }

    if ((slotlist & (1u << adc_slot_)) == 0) {
        std::cerr << "No potentiometer ADC data returned.\n";
        return -1;
    }

    short raw = static_cast<short>(
        buf[adc_slot_] & 0xFFFF
    );

    voltage = static_cast<double>(raw) * 10.0 / 32767.0;

    return S826_ERR_OK;
}

double Potentiometer::readPercent(double voltage)
{
    double percent =
        (voltage - min_voltage_) /
        (max_voltage_ - min_voltage_) * 100.0;

    return percent;
}