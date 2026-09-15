#include "pressure_controller.h"
#include "826api.h"

#include <cmath>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {
    constexpr double PI = 3.14159265358979323846;

    // Sensoray ADC configuration
    constexpr unsigned int VPPI_ADC_SLOT = 0;
    constexpr unsigned int VPPI_ADC_CHANNEL = 15;
    constexpr unsigned int ADC_SETTLING_TIME_US = 0;

    // VPPI actual-value output range
    constexpr double ACTUAL_OUTPUT_MIN_V = 0.0;
    constexpr double ACTUAL_OUTPUT_MAX_V = 10.0;

    // Convert pressure command to VPPI command voltage
    //
    // -1 bar -> 0 V
    //  0 bar -> 5 V
    // +1 bar -> 10 V
    double pressure_to_voltage(double pressure_bar) {
        const double pressure_fraction = (pressure_bar - min_pressure_bar) / (max_pressure_bar - min_pressure_bar);
        return min_command_voltage + pressure_fraction * (max_command_voltage - min_command_voltage);
    }

    // Convert VPPI actual-value voltage to pressure
    //
    // 0 V  -> -1 bar
    // 5 V  ->  0 bar
    // 10 V -> +1 bar
    double actual_voltage_to_pressure(double voltage) {
        const double voltage_fraction = (voltage - ACTUAL_OUTPUT_MIN_V) / (ACTUAL_OUTPUT_MAX_V - ACTUAL_OUTPUT_MIN_V);
        return min_pressure_bar + voltage_fraction * (max_pressure_bar - min_pressure_bar);
    }
}

// Generate ONE complete sine-wave pressure cycle
//
// Since our range is -1 to +1 bar:
//
// P = sin(phase)
//
// More generally:
//
// center = (max + min) / 2 = 0 bar
// amplitude = (max - min) / 2 = 1 bar
//
// This function ONLY creates pressure values.
// It does not control timing.
int s826_diagnostic(double* buf, int resolution) {
    if (buf == nullptr || resolution <= 0) {
        std::cerr << "Invalid diagnostic buffer or resolution.\n";
        return -1;
    }

    const double center_pressure = (max_pressure_bar + min_pressure_bar) / 2.0;
    const double amplitude = (max_pressure_bar - min_pressure_bar) / 2.0;

    for (int x = 0; x < resolution; ++x) {
        const double phase = 2.0 * PI * static_cast<double>(x) / static_cast<double>(resolution);
        buf[x] = center_pressure + amplitude * std::sin(phase);
    }

    return S826_ERR_OK;
}

// Pressure -> 16-bit Sensoray DAC value
//
// -1 bar -> 0 V  -> 0
//  0 bar -> 5 V  -> ~32768
// +1 bar -> 10 V -> 65535
unsigned int pressure_to_dac(double pressure_bar) {
    const double voltage = pressure_to_voltage(pressure_bar);
    const double normalized_voltage = voltage / max_command_voltage;
    return static_cast<unsigned int>(std::lround(normalized_voltage * 0xFFFF));
}

// Command a pressure
int s826_set_pressure(unsigned int board, unsigned int channel, double pressure_bar) {
    // THIS NOW ALLOWS NEGATIVE PRESSURE
    if (pressure_bar < min_pressure_bar || pressure_bar > max_pressure_bar) {
        std::cerr << "Pressure must be between " << min_pressure_bar << " and " << max_pressure_bar
            << " bar.\n";
        return -1;
    }

    const double voltage = pressure_to_voltage(pressure_bar);
    const unsigned int setpoint = pressure_to_dac(pressure_bar);
    const int status = S826_DacDataWrite(board, channel, setpoint, 0);

    if (status != S826_ERR_OK) {
        std::cerr << "S826_DacDataWrite failed: " << status << '\n';
        return status;
    }

    std::cout << "Pressure command : " << pressure_bar << " bar\n"
        << "Command voltage  : " << voltage << " V\n"
        << "DAC setpoint     : " << setpoint << " / 65535\n";
    return S826_ERR_OK;
}

// Initialize Sensoray ADC for VPPI actual-value feedback
//
// Wiring:
//
// FESTO pin 5 -> Sensoray +AIN15
// FESTO pin 2 -> Sensoray -AIN15
//
// ADC slot 0 is configured to read physical AIN15.
int s826_init_vppi_adc(unsigned int board) {
    int status;

    // Configure slot 0 to read physical AIN15.
    //
    // S826_ADC_GAIN_1 = +/-10 V measurement range.
    status = S826_AdcSlotConfigWrite(board, VPPI_ADC_SLOT, VPPI_ADC_CHANNEL, ADC_SETTLING_TIME_US,
        S826_ADC_GAIN_1);

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcSlotConfigWrite failed: " << status << '\n';
        return status;
    }

    // Enable slot 0 only.
    status = S826_AdcSlotlistWrite(board, (1u << VPPI_ADC_SLOT), S826_BITWRITE);

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcSlotlistWrite failed: " << status << '\n';
        return status;
    }

    // Continuous conversion mode.
    status = S826_AdcTrigModeWrite(board, 0);

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcTrigModeWrite failed: " << status << '\n';
        return status;
    }

    // Enable/start ADC conversions.
    status = S826_AdcEnableWrite(board, 1);

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcEnableWrite failed: " << status << '\n';
        return status;
    }

    std::cout << "\nVPPI ADC initialized successfully.\n"
        << "  ADC slot       : " << VPPI_ADC_SLOT << '\n'
        << "  AIN channel    : " << VPPI_ADC_CHANNEL << '\n'
        << "  ADC range      : +/-10 V\n"
        << "  Settling time  : " << ADC_SETTLING_TIME_US << " us\n";
    return S826_ERR_OK;
}

// Read VPPI actual-value pressure
int s826_read_pressure(unsigned int board, double& voltage, double& pressure_bar, unsigned int response_wait_ms) {
    int buf[16] = {};
    uint slotlist = (1u << VPPI_ADC_SLOT);

    // Throw away an old unread ADC sample if one exists.
    //
    // This helps prevent reading a sample that was generated
    // before our most recent pressure command.
    int pending_status = S826_AdcRead(board, buf, nullptr, &slotlist, 0);

    if (pending_status != S826_ERR_OK && pending_status != S826_ERR_MISSEDTRIG &&
        pending_status != S826_ERR_NOTREADY) {
        std::cerr << "Initial S826_AdcRead failed: " << pending_status << '\n';
        return pending_status;
    }

    // Give the pressure system time to respond.
    if (response_wait_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(response_wait_ms));
    }

    // Request slot 0 again.
    slotlist = (1u << VPPI_ADC_SLOT);
    buf[VPPI_ADC_SLOT] = 0;

    // Read a fresh ADC sample.
    int status = S826_AdcRead(board, buf, nullptr, &slotlist, 1000);

    if (status != S826_ERR_OK && status != S826_ERR_MISSEDTRIG) {
        std::cerr << "S826_AdcRead failed: " << status << '\n';
        return status;
    }

    // Verify slot 0 actually returned data.
    if ((slotlist & (1u << VPPI_ADC_SLOT)) == 0) {
        std::cerr << "ADC read returned no data for slot " << VPPI_ADC_SLOT << ".\n";
        return -1;
    }

    // Extract the signed 16-bit ADC measurement.
    //
    // The ADC result is stored in the LOWEST 16 bits of buf[].
    const short raw = static_cast<short>(buf[VPPI_ADC_SLOT] & 0xFFFF);

    // Convert raw ADC -> volts.
    //
    // S826_ADC_GAIN_1 is +/-10 V.
    //
    // Sensoray's documented conversion uses 0x7FFF = 32767.
    voltage = static_cast<double>(raw) * 10.0 / 32767.0;

    // Convert VPPI 0-10 V actual-value output -> -1 to +1 bar
    pressure_bar = actual_voltage_to_pressure(voltage);

    std::cout << "ADC raw value    : " << raw << '\n'
        << "Actual voltage   : " << voltage << " V\n"
        << "Actual pressure  : " << pressure_bar << " bar\n";

    // This doesn't stop execution; it alerts us to something
    // electrically unexpected.
    if (voltage < -0.1 || voltage > 10.1) {
        std::cerr << "WARNING: VPPI actual-value voltage is " << "outside the expected 0-10 V range.\n";
    }

    return S826_ERR_OK;
}

// Sensoray + VPPI initialization
int s826_init(unsigned int& board, unsigned int& dac_channel) {
    int board_nums[16] = {};
    int board_count = 0;
    std::string initialization_mode;

    std::cout << "-------------------" << "s826_init() called" << "-------------------\n";

    // Open Sensoray system
    int system_status = S826_SystemOpen();

    if (system_status < 0) {
        std::cerr << "Error opening Sensoray system: " << system_status << '\n';
        return system_status;
    }

    if (system_status == 0) {
        std::cerr << "No Sensoray 826 boards detected.\n";
        return -1;
    }

    // Detect boards
    for (int board_num = 0; board_num < 16; ++board_num) {
        if (system_status & (1 << board_num)) {
            std::cout << "Board " << board_num << " detected.\n";
            board_nums[board_count] = board_num;
            ++board_count;
        }
    }

    // Initialization selection
    std::cout << "\nEnter \"default\" for default initialization "
        << "or anything else for custom initialization: ";
    std::cin >> initialization_mode;

    if (!std::cin) {
        std::cerr << "Invalid initialization input.\n";
        return -1;
    }

    double initial_pressure = 0.0;

    if (initialization_mode == "default") {
        board = static_cast<unsigned int>(board_nums[0]);
        dac_channel = 0;

        // IMPORTANT:
        // For this +/-1 bar VPPI, 0 bar corresponds to 5 V.
        initial_pressure = 0.0;

        std::cout << "Default initialization selected.\n";
    } else {
        std::cout << "Board number: ";
        std::cin >> board;

        std::cout << "DAC channel (0-7): ";
        std::cin >> dac_channel;

        std::cout << "Initial pressure (" << min_pressure_bar << " to +" << max_pressure_bar << " bar): ";
        std::cin >> initial_pressure;

        if (!std::cin) {
            std::cerr << "Invalid initialization input.\n";
            return -1;
        }

        if (board > 15 || !(system_status & (1 << board))) {
            std::cerr << "Invalid or undetected board.\n";
            return -1;
        }

        if (dac_channel > 7) {
            std::cerr << "Invalid DAC channel.\n";
            return -1;
        }

        if (initial_pressure < min_pressure_bar || initial_pressure > max_pressure_bar) {
            std::cerr << "Pressure must be between " << min_pressure_bar << " and " << max_pressure_bar
                << " bar.\n";
            return -1;
        }
    }

    // Configure Sensoray DAC for 0-10 V output
    int status = S826_DacRangeWrite(board, dac_channel, S826_DAC_SPAN_0_10, 0);

    if (status != S826_ERR_OK) {
        std::cerr << "S826_DacRangeWrite failed: " << status << '\n';
        return status;
    }

    std::cout << "DAC channel " << dac_channel << " configured for 0-10 V.\n";

    // Immediately command the desired starting pressure.
    //
    // Default 0 bar -> 5 V.
    status = s826_set_pressure(board, dac_channel, initial_pressure);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to set initial VPPI pressure.\n";
        return status;
    }

    // Initialize VPPI feedback ADC
    status = s826_init_vppi_adc(board);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to initialize VPPI ADC.\n";
        return status;
    }

    // Verify actual pressure
    double actual_voltage = 0.0;
    double actual_pressure = 0.0;
    status = s826_read_pressure(board, actual_voltage, actual_pressure, 250);

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to read VPPI actual pressure.\n";
        return status;
    }

    std::cout << "\n"
        << "========== Sensoray / VPPI Initialization ==========\n"
        << "VPPI model         : " << "VPPI-5L-3-G18-1V1H-V1-S1D\n"
        << "Board              : " << board << '\n'
        << "DAC channel        : " << dac_channel << '\n'
        << "Pressure range     : " << min_pressure_bar << " to +" << max_pressure_bar << " bar\n"
        << "Command range      : 0 to 10 V\n"
        << "Commanded pressure : " << initial_pressure << " bar\n"
        << "Actual voltage     : " << actual_voltage << " V\n"
        << "Actual pressure    : " << actual_pressure << " bar\n"
        << "=====================================================\n";
    return S826_ERR_OK;
}
