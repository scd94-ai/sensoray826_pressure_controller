#include <iostream>
#include <string>
#include "826api.h"

int s826_diagnostic(){
    std::cout << "-------------------s826_diagnostic() called-------------------\n" << std::endl;
    std::string user_input;
    return 0;
}
int s826_init() {
    int board_nums[16];
    int board_count = 0;
    std::string user_initialization_input;

    std::cout << "-------------------s826_init() called-------------------\n";

    // Open the system and check for connected boards
    int status = S826_SystemOpen();

    if (status < 0) {
        std::cerr << "Error opening system: " << status << std::endl;
        return status;
    }
    else if (status == 0) {
        std::cerr << "No boards found. Please check the connection and try again.\n";
        return -1;
    }

    // Store detected board numbers
    for (int boardNum = 0; boardNum < 16; ++boardNum) {
        if (status & (1 << boardNum)) {
            std::cout << "Board " << boardNum << " detected\n";
            board_nums[board_count] = boardNum;
            board_count++;
        }
    }

    // Configure DAC channel
    std::cout << "Enter \"default\" for default initialization "
                 "or anything else for custom initialization: ";
    std::cin >> user_initialization_input;

    if (user_initialization_input == "default") {

        std::cout << "Default initialization selected.\n";

        int range_write_status = S826_DacRangeWrite(
            board_nums[0],
            0,
            S826_DAC_SPAN_0_10,
            0
        );

        if (range_write_status != S826_ERR_OK) {
            std::cerr << "Error configuring DAC: "
                      << range_write_status << '\n';
            return range_write_status;
        }

        unsigned int setpoint =
            static_cast<unsigned int>((5.0 / 10.0) * 0xFFFF);

        int dac_status = S826_DacDataWrite(
            board_nums[0],
            0,
            setpoint,
            0
        );

        if (dac_status != S826_ERR_OK) {
            std::cerr << "Error setting DAC voltage: "
                      << dac_status << '\n';
            return dac_status;
        }

        std::cout << "Board " << board_nums[0]
                  << ", DAC channel 0 set to 5 V.\n";
    }
    else {

        unsigned int board;
        unsigned int channel;
        double voltage;

        std::cout << "Board number: ";
        std::cin >> board;

        std::cout << "DAC channel (0-7): ";
        std::cin >> channel;

        std::cout << "Voltage (0-10 V): ";
        std::cin >> voltage;

        if (board > 15 || !(status & (1 << board))) {
            std::cerr << "Invalid or undetected board.\n";
            return -1;
        }

        if (channel > 7 || voltage < 0.0 || voltage > 10.0) {
            std::cerr << "Invalid channel or voltage.\n";
            return -1;
        }

        // Configure selected DAC channel for 0-10 V
        int dac_status = S826_DacRangeWrite(
            board,
            channel,
            S826_DAC_SPAN_0_10,
            0
        );

        if (dac_status != S826_ERR_OK) {
            std::cerr << "Error configuring DAC: "
                      << dac_status << '\n';
            return dac_status;
        }

        // Convert voltage to 16-bit DAC setpoint
        unsigned int setpoint =
            static_cast<unsigned int>((voltage / 10.0) * 0xFFFF);

        dac_status = S826_DacDataWrite(
            board,
            channel,
            setpoint,
            0
        );

        if (dac_status != S826_ERR_OK) {
            std::cerr << "Error setting DAC voltage: "
                      << dac_status << '\n';
            return dac_status;
        }

        std::cout << "Board " << board
                  << ", DAC channel " << channel
                  << " set to " << voltage << " V.\n";
    }

    return 0;
}


