#include "Festo.h"
#include "826api.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace {
    constexpr double PI = 3.14159265358979323846;
}

Festo::Festo(
    Sensoray826& sensoray,
    unsigned int dac_channel,
    unsigned int adc_channel,
    unsigned int adc_slot,
    double min_pressure_kpa,
    double max_pressure_kpa,
    double min_voltage,
    double max_voltage
)
    : sensoray_(sensoray),
      dac_channel_(dac_channel),
      adc_channel_(adc_channel),
      adc_slot_(adc_slot),
      min_pressure_kpa_(min_pressure_kpa),
      max_pressure_kpa_(max_pressure_kpa),
      min_voltage_(min_voltage),
      max_voltage_(max_voltage)
{
}

double Festo::pressureToVoltage(double pressure_kpa) const
{
    const double fraction =
        (pressure_kpa - min_pressure_kpa_) /
        (max_pressure_kpa_ - min_pressure_kpa_);

    return min_voltage_ + fraction * (max_voltage_ - min_voltage_);
}

double Festo::voltageToPressure(double voltage) const
{
    const double fraction =
        (voltage - min_voltage_) /
        (max_voltage_ - min_voltage_);

    return min_pressure_kpa_ +
           fraction * (max_pressure_kpa_ - min_pressure_kpa_);
}

unsigned int Festo::pressureToDac(double pressure_kpa) const
{
    const double voltage = pressureToVoltage(pressure_kpa);
    const double normalized_voltage =
        (voltage - min_voltage_) / (max_voltage_ - min_voltage_);

    return static_cast<unsigned int>(
        std::lround(normalized_voltage * 0xFFFF)
    );
}

int Festo::initialize()
{
    int status = sensoray_.configureDac(dac_channel_);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to configure Festo DAC channel "
                  << dac_channel_ << ": " << status << '\n';
        return status;
    }

    status = sensoray_.configureAdcSlot(adc_slot_, adc_channel_);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to configure Festo ADC slot "
                  << adc_slot_ << ": " << status << '\n';
        return status;
    }

    status = setPressure(0.0);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to set initial Festo pressure to 0 kPa.\n";
        return status;
    }

    std::cout << "Festo initialized."
              << " DAC=" << dac_channel_
              << " ADC=" << adc_channel_
              << " slot=" << adc_slot_ << '\n';

    return S826_ERR_OK;
}

int Festo::setPressure(double pressure_kpa)
{
    if (pressure_kpa < min_pressure_kpa_ ||
        pressure_kpa > max_pressure_kpa_) {
        std::cerr << "Pressure must be between "
                  << min_pressure_kpa_ << " and "
                  << max_pressure_kpa_ << " kPa.\n";
        return -1;
    }

    const double voltage = pressureToVoltage(pressure_kpa);
    const unsigned int setpoint = pressureToDac(pressure_kpa);

    const int status =
        sensoray_.writeDac(dac_channel_, setpoint);

    if (status != S826_ERR_OK) {
        std::cerr << "DAC write failed: " << status << '\n';
        return status;
    }

    std::cout << "Pressure command : " << pressure_kpa << " kPa\n"
              << "Command voltage  : " << voltage << " V\n"
              << "DAC setpoint     : " << setpoint << " / 65535\n";

    return S826_ERR_OK;
}

int Festo::readPressure(
    double& voltage,
    double& pressure_kpa,
    unsigned int response_wait_ms
)
{
    int status = sensoray_.discardAdcSample(adc_slot_);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to discard old ADC sample: "
                  << status << '\n';
        return status;
    }

    if (response_wait_ms > 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(response_wait_ms)
        );
    }

    status = sensoray_.readAdcVoltage(
        adc_slot_,
        voltage,
        1000
    );

    if (status != S826_ERR_OK) {
        std::cerr << "Festo ADC read failed: "
                  << status << '\n';
        return status;
    }

    pressure_kpa = voltageToPressure(voltage);

    std::cout << "Actual voltage  : " << voltage << " V\n"
              << "Actual pressure : " << pressure_kpa << " kPa\n";

    return S826_ERR_OK;
}

int Festo::generateSineWave(double* buf, int resolution) const
{
    if (buf == nullptr || resolution <= 0) return -1;

    const double center =
        (max_pressure_kpa_ + min_pressure_kpa_) / 2.0;

    const double amplitude =
        (max_pressure_kpa_ - min_pressure_kpa_) / 2.0;

    for (int i = 0; i < resolution; ++i) {
        const double phase =
            2.0 * PI * static_cast<double>(i) /
            static_cast<double>(resolution);

        buf[i] = center + amplitude * std::sin(phase);
    }

    return S826_ERR_OK;
}

int Festo::runDiagnostic()
{
    constexpr int resolution = 30;
    constexpr int cycles = 2;
    constexpr double updates_per_second = 10.0;

    double pressures_kpa[resolution];

    int status =
        generateSineWave(pressures_kpa, resolution);

    if (status != S826_ERR_OK) return status;

    const auto start_time =
        std::chrono::steady_clock::now();

    for (int cycle = 0; cycle < cycles; ++cycle) {
        for (int i = 0; i < resolution; ++i) {
            const int sample_number =
                cycle * resolution + i;

            const auto target_time =
                start_time +
                std::chrono::duration_cast<
                    std::chrono::steady_clock::duration
                >(
                    std::chrono::duration<double>(
                        sample_number / updates_per_second
                    )
                );

            std::this_thread::sleep_until(target_time);

            status = setPressure(pressures_kpa[i]);
            if (status != S826_ERR_OK) return status;

            double voltage = 0.0;
            double pressure_kpa = 0.0;

            status = readPressure(
                voltage,
                pressure_kpa,
                100
            );

            if (status != S826_ERR_OK) return status;
        }
    }

    return setPressure(0.0);
}
