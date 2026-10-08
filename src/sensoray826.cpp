#include "Sensoray826.h"
#include "826api.h"

#include <iostream>

namespace {
    constexpr unsigned int ADC_SETTLING_TIME_US = 0;
}

Sensoray826::Sensoray826(unsigned int board_num)
    : board_num_(board_num),
      system_open_(false),
      adc_running_(false)
{
}

Sensoray826::~Sensoray826()
{
    close();
}

int Sensoray826::initialize()
{
    if (system_open_) return S826_ERR_OK;

    int system_status = S826_SystemOpen();

    if (system_status < 0) {
        std::cerr << "Error opening Sensoray system: "
                  << system_status << '\n';
        return system_status;
    }

    if (system_status == 0) {
        std::cerr << "No Sensoray 826 boards detected.\n";
        return -1;
    }

    if (!(system_status & (1u << board_num_))) {
        std::cerr << "Sensoray board " << board_num_
                  << " was not detected.\n";
        S826_SystemClose();
        return -1;
    }

    system_open_ = true;

    std::cout << "Sensoray 826 initialized on board "
              << board_num_ << ".\n";

    return S826_ERR_OK;
}

int Sensoray826::close()
{
    if (!system_open_) return S826_ERR_OK;

    int stop_status = stopAdc();
    int close_status = S826_SystemClose();

    system_open_ = false;
    adc_running_ = false;

    if (stop_status != S826_ERR_OK) return stop_status;
    return close_status;
}

int Sensoray826::configureDac(unsigned int dac_channel)
{
    if (!system_open_ || dac_channel > 7) return -1;

    return S826_DacRangeWrite(
        board_num_,
        dac_channel,
        S826_DAC_SPAN_0_10,
        0
    );
}

int Sensoray826::writeDac(
    unsigned int dac_channel,
    unsigned int setpoint
)
{
    if (!system_open_ || dac_channel > 7) return -1;

    return S826_DacDataWrite(
        board_num_,
        dac_channel,
        setpoint,
        0
    );
}

int Sensoray826::configureAdcSlot(
    unsigned int adc_slot,
    unsigned int adc_channel
)
{
    if (!system_open_ || adc_slot > 15 || adc_channel > 15) {
        return -1;
    }

    int status = S826_AdcSlotConfigWrite(
        board_num_,
        adc_slot,
        adc_channel,
        ADC_SETTLING_TIME_US,
        S826_ADC_GAIN_1
    );

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcSlotConfigWrite failed: "
                  << status << '\n';
        return status;
    }

    status = S826_AdcSlotlistWrite(
        board_num_,
        (1u << adc_slot),
        S826_BITSET
    );

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcSlotlistWrite failed: "
                  << status << '\n';
    }

    return status;
}

int Sensoray826::startAdc()
{
    if (!system_open_) return -1;
    if (adc_running_) return S826_ERR_OK;

    int status = S826_AdcTrigModeWrite(board_num_, 0);
    if (status != S826_ERR_OK) return status;

    status = S826_AdcEnableWrite(board_num_, 1);

    if (status == S826_ERR_OK) {
        adc_running_ = true;
    }

    return status;
}

int Sensoray826::stopAdc()
{
    if (!system_open_ || !adc_running_) return S826_ERR_OK;

    int status = S826_AdcEnableWrite(board_num_, 0);

    if (status == S826_ERR_OK) {
        adc_running_ = false;
    }

    return status;
}

int Sensoray826::discardAdcSample(unsigned int adc_slot)
{
    if (!system_open_ || adc_slot > 15) return -1;

    int buf[16] = {};
    uint slotlist = (1u << adc_slot);

    int status = S826_AdcRead(
        board_num_,
        buf,
        nullptr,
        &slotlist,
        0
    );

    if (status == S826_ERR_OK ||
        status == S826_ERR_MISSEDTRIG ||
        status == S826_ERR_NOTREADY) {
        return S826_ERR_OK;
    }

    return status;
}

int Sensoray826::readAdcVoltage(
    unsigned int adc_slot,
    double& voltage,
    unsigned int timeout_ms
)
{
    if (!system_open_ || !adc_running_ || adc_slot > 15) {
        return -1;
    }

    int buf[16] = {};
    uint slotlist = (1u << adc_slot);

    int status = S826_AdcRead(
        board_num_,
        buf,
        nullptr,
        &slotlist,
        timeout_ms
    );

    if (status != S826_ERR_OK &&
        status != S826_ERR_MISSEDTRIG) {
        return status;
    }

    if ((slotlist & (1u << adc_slot)) == 0) {
        return -1;
    }

    const short raw =
        static_cast<short>(buf[adc_slot] & 0xFFFF);

    voltage =
        static_cast<double>(raw) * 10.0 / 32767.0;

    return S826_ERR_OK;
}

unsigned int Sensoray826::boardNumber() const
{
    return board_num_;
}

bool Sensoray826::isOpen() const
{
    return system_open_;
}
