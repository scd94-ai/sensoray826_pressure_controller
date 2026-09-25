#include "Festo.h"
#include "826api.h"
#include <cmath>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
    constexpr double PI = 3.14159265358979323846;
    constexpr unsigned int ADC_SETTLING_TIME_US = 0;
}

Festo::Festo(
    unsigned int board_num,
    unsigned int dac_channel,
    unsigned int adc_channel,
    unsigned int adc_slot,
    double min_pressure_bar,
    double max_pressure_bar,
    double min_voltage,
    double max_voltage
)
    : board_num_(board_num),
      dac_channel_(dac_channel),
      adc_channel_(adc_channel),
      adc_slot_(adc_slot),
      min_pressure_bar_(min_pressure_bar),
      max_pressure_bar_(max_pressure_bar),
      min_voltage_(min_voltage),
      max_voltage_(max_voltage)
{
}

double Festo::pressureToVoltage(double pressure_bar)
{
    double fraction = (pressure_bar - min_pressure_bar_) /
                      (max_pressure_bar_ - min_pressure_bar_);

    return min_voltage_ + fraction * (max_voltage_ - min_voltage_);
}

double Festo::voltageToPressure(double voltage)
{
    double fraction = (voltage - min_voltage_) /
                      (max_voltage_ - min_voltage_);

    return min_pressure_bar_ + fraction * (max_pressure_bar_ - min_pressure_bar_);
}

unsigned int Festo::pressureToDac(double pressure_bar)
{
    double voltage = pressureToVoltage(pressure_bar);
    double normalized_voltage = voltage / max_voltage_;

    return static_cast<unsigned int>(
        std::lround(normalized_voltage * 0xFFFF)
    );
}

int Festo::setPressure(double pressure_bar)
{
    if (pressure_bar < min_pressure_bar_ || pressure_bar > max_pressure_bar_) {
        std::cerr << "Pressure must be between "
                  << min_pressure_bar_ << " and "
                  << max_pressure_bar_ << " bar.\n";
        return -1;
    }

    double voltage = pressureToVoltage(pressure_bar);
    unsigned int setpoint = pressureToDac(pressure_bar);

    int status = S826_DacDataWrite(board_num_, dac_channel_, setpoint, 0);

    if (status != S826_ERR_OK) {
        std::cerr << "S826_DacDataWrite failed: " << status << '\n';
        return status;
    }

    std::cout << "Pressure command : " << pressure_bar << " bar\n"
              << "Command voltage  : " << voltage << " V\n"
              << "DAC setpoint     : " << setpoint << " / 65535\n";

    return S826_ERR_OK;
}

int Festo::initializeADC()
{
    int status = S826_AdcSlotConfigWrite(
        board_num_, adc_slot_, adc_channel_,
        ADC_SETTLING_TIME_US, S826_ADC_GAIN_1
    );

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcSlotConfigWrite failed: " << status << '\n';
        return status;
    }

    status = S826_AdcSlotlistWrite(
        board_num_, (1u << adc_slot_), S826_BITWRITE
    );

    if (status != S826_ERR_OK) return status;

    status = S826_AdcTrigModeWrite(board_num_, 0);
    if (status != S826_ERR_OK) return status;

    status = S826_AdcEnableWrite(board_num_, 1);
    return status;
}

int Festo::readPressure(
    double& voltage,
    double& pressure_bar,
    unsigned int response_wait_ms
)
{
    int buf[16] = {};
    uint slotlist = (1u << adc_slot_);

    int pending_status = S826_AdcRead(
        board_num_, buf, nullptr, &slotlist, 0
    );

    if (pending_status != S826_ERR_OK &&
        pending_status != S826_ERR_MISSEDTRIG &&
        pending_status != S826_ERR_NOTREADY) {
        std::cerr << "Initial S826_AdcRead failed: "
                  << pending_status << '\n';
        return pending_status;
    }

    if (response_wait_ms > 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(response_wait_ms)
        );
    }

    slotlist = (1u << adc_slot_);
    buf[adc_slot_] = 0;

    int status = S826_AdcRead(
        board_num_, buf, nullptr, &slotlist, 1000
    );

    if (status != S826_ERR_OK &&
        status != S826_ERR_MISSEDTRIG) {
        std::cerr << "S826_AdcRead failed: " << status << '\n';
        return status;
    }

    if ((slotlist & (1u << adc_slot_)) == 0) {
        std::cerr << "ADC read returned no data for slot "
                  << adc_slot_ << ".\n";
        return -1;
    }

    short raw = static_cast<short>(buf[adc_slot_] & 0xFFFF);

    voltage = static_cast<double>(raw) * 10.0 / 32767.0;
    pressure_bar = voltageToPressure(voltage);

    std::cout << "ADC raw value   : " << raw << '\n'
              << "Actual voltage  : " << voltage << " V\n"
              << "Actual pressure : " << pressure_bar << " bar\n";

    return S826_ERR_OK;
}

int Festo::generateSineWave(double* buf, int resolution)
{
    if (buf == nullptr || resolution <= 0) return -1;

    double center = (max_pressure_bar_ + min_pressure_bar_) / 2.0;
    double amplitude = (max_pressure_bar_ - min_pressure_bar_) / 2.0;

    for (int i = 0; i < resolution; ++i) {
        double phase = 2.0 * PI * i / resolution;
        buf[i] = center + amplitude * std::sin(phase);
    }

    return S826_ERR_OK;
}

int Festo::runDiagnostic()
{
    constexpr int resolution = 30;
    constexpr int cycles = 2;
    constexpr double updates_per_second = 3.0;

    double pressures[resolution];

    int status = generateSineWave(pressures, resolution);
    if (status != S826_ERR_OK) return status;

    auto start_time = std::chrono::steady_clock::now();

    for (int cycle = 0; cycle < cycles; ++cycle) {
        for (int i = 0; i < resolution; ++i) {
            int sample_number = cycle * resolution + i;

            auto target_time =
                start_time +
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(
                        sample_number / updates_per_second
                    )
                );

            std::this_thread::sleep_until(target_time);

            status = setPressure(pressures[i]);
            if (status != S826_ERR_OK) return status;

            double voltage = 0.0;
            double pressure = 0.0;

            status = readPressure(voltage, pressure, 100);
            if (status != S826_ERR_OK) return status;
        }
    }

    return setPressure(0.0);
}

int Festo::initialize()
{
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

    if (!(system_status & (1 << board_num_))) {
        std::cerr << "Sensoray board "
                  << board_num_ << " was not detected.\n";
        return -1;
    }

    int status = S826_DacRangeWrite(
        board_num_, dac_channel_, S826_DAC_SPAN_0_10, 0
    );

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to configure DAC.\n";
        return status;
    }

    status = initializeADC();
    if (status != S826_ERR_OK) {
        std::cerr << "Failed to initialize ADC.\n";
        return status;
    }

    status = setPressure(0.0);
    if (status != S826_ERR_OK) {
        std::cerr << "Failed to set initial pressure.\n";
        return status;
    }

    std::cout << "Festo initialized successfully.\n"
              << "Board: " << board_num_ << '\n'
              << "DAC: " << dac_channel_ << '\n'
              << "ADC: " << adc_channel_ << '\n'
              << "ADC slot: " << adc_slot_ << '\n';

    return S826_ERR_OK;
}