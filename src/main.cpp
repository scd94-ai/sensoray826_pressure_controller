#include "pressure_controller.h"
#include <iostream>
#include "826api.h"
#include <array>
#include <chrono>
#include <iomanip>
#include <thread>

int main()
{
    unsigned int board = 0;
    unsigned int dac_channel = 0;

    // Initialize Sensoray + VPPI
    int status = s826_init(board, dac_channel);

    if (status != S826_ERR_OK) {
        std::cerr << "Initialization failed with error: "
                  << status << '\n';

        S826_SystemClose();
        return 1;
    }

    double desired_pressure = 0.0;
    double actual_voltage = 0.0;
    double actual_pressure = 0.0;

    while (true) {
        std::cout << "\n1. Set pressure manually\n"
                  << "2. Diagnostic (0-6 bar sine wave, 10 seconds, 3 updates/sec)\n"
                  << "3. Quit\n"
                  << "Select option: ";

        int option = 0;
        if (!(std::cin >> option)) {
            if (!std::cin.eof()) {
                std::cerr << "Invalid input.\n";
                status = -1;
            }
            break;
        }

        if (option == 3) {
            break;
        }

        if (option == 2) {
            constexpr int sample_count = 30;
            constexpr double updates_per_second = 3.0;
            std::array<double, sample_count> samples{};
            status = s826_diagnostic(samples.data(), sample_count);
            if (status != S826_ERR_OK) {
                std::cerr << "Failed to generate diagnostic samples.\n";
                break;
            }

            std::cout << "\n========== Pressure Diagnostic ==========\n"
                      << "Board: " << board << "  |  DAC channel: " << dac_channel << '\n'
                      << "Range: 0-6 bar  |  Duration: 10 seconds\n"
                      << "Sine cycle: 0.1 Hz  |  Updates: 3 per second\n"
                      << "Values below are commanded outputs.\n"
                      << "=========================================\n";

            const auto output_flags = std::cout.flags();
            const auto output_precision = std::cout.precision();
            std::cout << std::fixed << std::setprecision(3);

            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < sample_count; ++i) {
                std::this_thread::sleep_until(
                    start + std::chrono::duration<double>(i / updates_per_second));
                std::cout << "\n--- Sample " << std::setw(2) << i + 1
                          << '/' << sample_count << " | Scheduled time: "
                          << i / updates_per_second << " s ---\n";
                status = s826_set_pressure(board, dac_channel, samples[i]);
                if (status != S826_ERR_OK) {
                    std::cerr << "Diagnostic pressure command failed.\n";
                    break;
                }
            }

            std::cout.flags(output_flags);
            std::cout.precision(output_precision);

            if (status != S826_ERR_OK) {
                break; // Use the common shutdown path to attempt zero pressure.
            }

            std::this_thread::sleep_until(
                start + std::chrono::duration<double>(sample_count / updates_per_second));
            std::cout << "\n--- End of test: returning pressure to zero ---\n";
            status = s826_set_pressure(board, dac_channel, 0.0);
            if (status != S826_ERR_OK) {
                break;
            }
            std::cout << "\n========== Diagnostic Complete ==========\n"
                      << "30 samples sent. Pressure set to 0 bar.\n"
                      << "Returning to the main menu.\n"
                      << "=========================================\n";
            continue;
        }

        if (option != 1) {
            std::cerr << "Select 1, 2, or 3.\n";
            continue;
        } 

        std::cout
            << "\nEnter desired pressure in bar "
            << "(0-" << max_pressure_bar
            << "), or -1 to quit: ";

        std::cin >> desired_pressure;

        if (!std::cin) {
            if (!std::cin.eof()) {
                std::cerr << "Invalid input.\n";
                status = -1;
            }
            break;
        }

        if (desired_pressure == -1.0) {
            break;
        }

        if (desired_pressure < 0.0 ||
            desired_pressure > max_pressure_bar) {

            std::cerr
                << "Pressure must be between 0 and "
                << max_pressure_bar
                << " bar.\n";

            continue;
        }

        // Command pressure
        status = s826_set_pressure(
            board,
            dac_channel,
            desired_pressure
        );

        if (status != S826_ERR_OK) {
            std::cerr
                << "Failed to set pressure.\n";

            break;
        }

        // Read pressure feedback
        status = s826_read_pressure(
            board,
            actual_voltage,
            actual_pressure
        );

        if (status != S826_ERR_OK) {
            std::cerr
                << "Failed to read pressure.\n";

            break;
        }

        std::cout
            << "\nCommanded pressure: "
            << desired_pressure << " bar\n"
            << "Actual pressure   : "
            << actual_pressure << " bar\n"
            << "Actual voltage    : "
            << actual_voltage << " V\n";
    }

    // Set pressure to 0 before exiting
    std::cout << "\nSetting VPPI pressure to 0 bar...\n";

    int shutdown_status = s826_set_pressure(
        board,
        dac_channel,
        0.0
    );

    if (shutdown_status != S826_ERR_OK) {
        std::cerr << "Failed to set pressure to zero: "
                  << shutdown_status << '\n';
        status = shutdown_status;
    }

    // Close Sensoray system
    S826_SystemClose();

    std::cout << "System closed.\n";

    return status == S826_ERR_OK ? 0 : 1;
}
