
#include "SMC.h"
#include "826api.h"

#include <cmath>
#include <iostream>

namespace {
    constexpr double MIN_PRESSURE_KPA = -100.0;
    constexpr double MAX_PRESSURE_KPA = -1.0;

    constexpr double MIN_VOLTAGE = 0.0;
    constexpr double MAX_VOLTAGE = 10.0;
}

// Constructor
SMC::SMC(
    Sensoray826& sensoray,
    unsigned int dac_channel
)
    : Sensoray826_Device(sensoray),
      dac_channel_(dac_channel)
{
}

// Convert requested pressure to command voltage
double SMC::pressureToVoltage(double pressure_kpa) const
{
    const double fraction =
        (MAX_PRESSURE_KPA - pressure_kpa) /
        (MAX_PRESSURE_KPA - MIN_PRESSURE_KPA);

    return MIN_VOLTAGE +
           fraction * (MAX_VOLTAGE - MIN_VOLTAGE);
}

// Initialize regulator
int SMC::initialize()
{
    int status = configureDAC(dac_channel_);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to configure SMC DAC channel "
                  << dac_channel_ << ": " << status << '\n';
        return status;
    }

    status = reset();

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to initialize SMC command voltage.\n";
        return status;
    }

    std::cout << "SMC initialized on DAC channel "
              << dac_channel_ << '\n';

    return S826_ERR_OK;
}

// Command vacuum pressure
int SMC::setPressure(double pressure_kpa)
{
    if (!std::isfinite(pressure_kpa) ||
        pressure_kpa < MIN_PRESSURE_KPA ||
        pressure_kpa > MAX_PRESSURE_KPA) {

        std::cerr << "SMC pressure must be between "
                  << MIN_PRESSURE_KPA << " and "
                  << MAX_PRESSURE_KPA << " kPa.\n";

        return -1;
    }

    const double voltage = pressureToVoltage(pressure_kpa);

    const int status = writeVoltage(dac_channel_, voltage);

    if (status != S826_ERR_OK) {
        std::cerr << "SMC DAC write failed: "
                  << status << '\n';
        return status;
    }

    std::cout << "SMC pressure command: "
              << pressure_kpa << " kPa\n"
              << "Command voltage: "
              << voltage << " V\n";

    return S826_ERR_OK;
}

// Command minimum vacuum
int SMC::reset()
{
    return writeVoltage(dac_channel_, MIN_VOLTAGE);
}
