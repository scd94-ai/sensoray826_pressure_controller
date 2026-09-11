#define _USE_MATH_DEFINES

#include "pressure_controller.h"
#include "826api.h"

#include <iostream>
#include <string>
#include <cmath>

constexpr double max_voltage = 10.0; // Maximum voltage in volts (This can be changed by changing the DAC range in initialization currently 0-10V)


int s826_diagnostic(double* buf, int resolution)
{
    if (buf == nullptr || resolution <= 0) {
        return -1;
    }

    for (int x = 0; x < resolution; x++) {

        double t =
            static_cast<double>(x) / resolution;

        buf[x] =
            (max_pressure_bar / 2.0) * std::sin(2.0 * M_PI * t)
            + max_pressure_bar / 2.0;
    }

    return 0;
}

unsigned int pressure_to_dac(double pressure_bar){
    double voltage = (pressure_bar / max_pressure_bar) * max_voltage;
    unsigned int setpoint =
    static_cast<unsigned int>(
        (voltage / 10.0) * 0xFFFF
    );
    return setpoint;
}

int s826_set_pressure(
    unsigned int board,
    unsigned int channel,
    double pressure_bar)
{
    if (pressure_bar < 0.0 || pressure_bar > max_pressure_bar) {
        std::cerr << "Pressure must be between 0 and "
                  << max_pressure_bar << " bar.\n";
        return -1;
    }

    unsigned int setpoint = pressure_to_dac(pressure_bar);

    int data_write_status =
        S826_DacDataWrite(
            board,
            channel,
            setpoint,
            0
        );

    if (data_write_status != S826_ERR_OK) {
        std::cerr << "Error setting DAC voltage: "
                  << data_write_status << '\n';

        return data_write_status;
    }

    double voltage =
        (pressure_bar / max_pressure_bar) * max_voltage;

    std::cout
        << "Pressure command: " << pressure_bar << " bar\n"
        << "Command voltage : " << voltage << " V\n"
        << "DAC setpoint    : " << setpoint << '\n';

    return S826_ERR_OK;
}

int s826_init_vppi_adc(unsigned int board){
    constexpr unsigned int adc_slot = 0; 
    constexpr unsigned int adc_channel = 15; // Assuming channel 15 is used for VPPI
    constexpr unsigned int settling_time_us = 0; // Assuming 0 us settling time for VPPI
    int status;

    status = S826_AdcSlotConfigWrite(board, adc_slot, adc_channel, settling_time_us, S826_ADC_GAIN_1);
    if (status != S826_ERR_OK) {
        std::cerr
            << "S826_AdcSlotConfigWrite failed: "
            << status << '\n';

        return status;
    }

  status = S826_AdcSlotlistWrite(
        board,
        (1u << adc_slot),
        S826_BITWRITE
    );

    if (status != S826_ERR_OK) {
        std::cerr
            << "S826_AdcSlotlistWrite failed: "
            << status << '\n';

        return status;
    }

    // Continuous conversion mode.
    status = S826_AdcTrigModeWrite(
        board,
        0
    );

    if (status != S826_ERR_OK) {
        std::cerr
            << "S826_AdcTrigModeWrite failed: "
            << status << '\n';

        return status;
    }

    // Start ADC conversions.
    status = S826_AdcEnableWrite(
        board,
        1
    );

    if (status != S826_ERR_OK) {
        std::cerr
            << "S826_AdcEnableWrite failed: "
            << status << '\n';

        return status;
    }

    std::cout
        << "VPPI ADC initialized successfully.\n"
        << "  Slot:          " << adc_slot << '\n'
        << "  AIN channel:   " << adc_channel << '\n'
        << "  Input range:   +/-10 V\n"
        << "  Settling time: " << settling_time_us << " us\n";

    return S826_ERR_OK;

}

int s826_read_pressure(
    unsigned int board,
    double& voltage,
    double& pressure_bar)
{
    int buf[16] = {};
    uint slotlist = 1u << 0;   // request ADC slot 0

    int status = S826_AdcRead(
        board,
        buf,
        nullptr,               // no timestamps needed
        &slotlist,
        S826_WAIT_INFINITE
    );

    if (status != S826_ERR_OK &&
        status != S826_ERR_MISSEDTRIG) {
        
        std::cerr << "S826_AdcRead failed: "
                  << status << '\n';

        return status;
    }

    // Slot 0 contains the reading from AIN15
    short raw =
        static_cast<short>(buf[0] & 0xFFFF);

    // Convert signed 16-bit ADC result to volts
    // using the +/-10 V range
    voltage =
        static_cast<double>(raw)
        * 10.0 / 32768.0;

    // Convert 0-10 V feedback into 0-6 bar
    pressure_bar =
        (voltage / 10.0) * 6.0;

    std::cout
        << "ADC raw value   : " << raw << '\n'
        << "Actual voltage : " << voltage << " V\n"
        << "Actual pressure: " << pressure_bar << " bar\n";

    return S826_ERR_OK;
}



int s826_init(unsigned int& board, unsigned int& dac_channel)
{
    int board_nums[16] = {};
    int board_count = 0;

    std::string user_initialization_input;

    std::cout
        << "-------------------s826_init() called-------------------\n";

    // --------------------------------------------------------
    // Open Sensoray system
    // --------------------------------------------------------

    int system_status = S826_SystemOpen();

    if (system_status < 0) {
        std::cerr
            << "Error opening Sensoray system: "
            << system_status << '\n';

        return system_status;
    }

    if (system_status == 0) {
        std::cerr
            << "No Sensoray boards found.\n";

        return -1;
    }

    // --------------------------------------------------------
    // Find connected boards
    // --------------------------------------------------------

    for (int board_num = 0; board_num < 16; ++board_num) {

        if (system_status & (1 << board_num)) {

            std::cout
                << "Board "
                << board_num
                << " detected.\n";

            board_nums[board_count] = board_num;
            ++board_count;
        }
    }

    // --------------------------------------------------------
    // Ask for initialization mode
    // --------------------------------------------------------

    std::cout
        << "Enter \"default\" for default initialization "
        << "or anything else for custom initialization: ";

    std::cin >> user_initialization_input;

    double pressure = 0.0;

    // --------------------------------------------------------
    // Default configuration
    // --------------------------------------------------------

    if (user_initialization_input == "default") {

        std::cout
            << "Default initialization selected.\n";

        board = board_nums[0];
        dac_channel = 0;

        // Safer startup pressure
        pressure = 0.0;
    }

    // --------------------------------------------------------
    // Custom configuration
    // --------------------------------------------------------

    else {

        std::cout << "Board number: ";
        std::cin >> board;

        std::cout << "DAC channel (0-7): ";
        std::cin >> dac_channel;

        std::cout
            << "Initial pressure (0-"
            << max_pressure_bar
            << " bar): ";

        std::cin >> pressure;

        if (!std::cin) {
            std::cerr << "Invalid initialization input.\n";
            return -1;
        }

        // Validate board
        if (board > 15 ||
            !(system_status & (1 << board))) {

            std::cerr
                << "Invalid or undetected board.\n";

            return -1;
        }

        // Validate DAC channel
        if (dac_channel > 7) {

            std::cerr
                << "Invalid DAC channel.\n";

            return -1;
        }

        // Validate pressure
        if (pressure < 0.0 ||
            pressure > max_pressure_bar) {

            std::cerr
                << "Invalid pressure.\n";

            return -1;
        }
    }

    // --------------------------------------------------------
    // Configure DAC range
    // --------------------------------------------------------

    int status =
        S826_DacRangeWrite(
            board,
            dac_channel,
            S826_DAC_SPAN_0_10,
            0
        );

    if (status != S826_ERR_OK) {

        std::cerr
            << "S826_DacRangeWrite failed: "
            << status << '\n';

        return status;
    }

    std::cout
        << "DAC channel "
        << dac_channel
        << " configured for 0-10 V.\n";

    // --------------------------------------------------------
    // Set initial VPPI pressure
    // --------------------------------------------------------

    status =
        s826_set_pressure(
            board,
            dac_channel,
            pressure
        );

    if (status != S826_ERR_OK) {

        std::cerr
            << "Failed to set initial VPPI pressure.\n";

        return status;
    }

    // --------------------------------------------------------
    // Initialize ADC for VPPI actual-value feedback
    // --------------------------------------------------------

    status =
        s826_init_vppi_adc(board);

    if (status != S826_ERR_OK) {

        std::cerr
            << "Failed to initialize VPPI ADC.\n";

        return status;
    }

    // --------------------------------------------------------
    // Read actual pressure once for verification
    // --------------------------------------------------------

    double actual_voltage = 0.0;
    double actual_pressure = 0.0;

    status =
        s826_read_pressure(
            board,
            actual_voltage,
            actual_pressure
        );

    if (status != S826_ERR_OK) {

        std::cerr
            << "Failed to read VPPI actual pressure.\n";

        return status;
    }

    // --------------------------------------------------------
    // Initialization summary
    // --------------------------------------------------------

    std::cout
        << "\n"
        << "---------- Sensoray / VPPI Initialization ----------\n"
        << "Board              : " << board << '\n'
        << "DAC channel        : " << dac_channel << '\n'
        << "Commanded pressure : " << pressure << " bar\n"
        << "Actual voltage     : " << actual_voltage << " V\n"
        << "Actual pressure    : " << actual_pressure << " bar\n"
        << "-----------------------------------------------------\n";

    return S826_ERR_OK;
}
