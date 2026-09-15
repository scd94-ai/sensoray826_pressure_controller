#include "pressure_controller.h"
#include "826api.h"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

int main() {
    unsigned int board = 0;
    unsigned int dac_channel = 0;

    // Initialize Sensoray + VPPI
    int status = s826_init(board, dac_channel);

    if (status != S826_ERR_OK) {
        std::cerr << "Initialization failed with error: " << status << '\n';
        S826_SystemClose();
        return 1;
    }

    double desired_pressure = 0.0;
    double actual_voltage = 0.0;
    double actual_pressure = 0.0;

    // Main program loop
    while (true) {
        std::cout << "\n"
            << "================ VPPI Controller ================\n"
            << "1. Set pressure manually\n"
            << "2. Diagnostic (-1 to +1 bar sine, " "2 cycles, 3 updates/sec)\n"
            << "3. Quit\n"
            << "=================================================\n"
            << "Select option: ";
        int option = 0;

        if (!(std::cin >> option)) {
            if (!std::cin.eof()) {
                std::cerr << "Invalid input.\n";
                status = -1;
            }

            break;
        }

        // QUIT
        if (option == 3) {
            break;
        }

        // DIAGNOSTIC
        if (option == 2) {
            constexpr int samples_per_cycle = 30;
            constexpr int number_of_cycles = 2;
            constexpr double updates_per_second = 3.0;
            constexpr int total_updates = samples_per_cycle * number_of_cycles;
            constexpr double sine_frequency_hz = updates_per_second / static_cast<double>(samples_per_cycle);
            constexpr double test_duration_seconds = total_updates / updates_per_second;
            std::array<double, samples_per_cycle> samples{};

            // Generate ONE complete sine cycle.
            status = s826_diagnostic(samples.data(), samples_per_cycle);

            if (status != S826_ERR_OK) {
                std::cerr << "Failed to generate " "diagnostic samples.\n";
                break;
            }

            std::cout << "\n"
                << "============== Pressure Diagnostic ==============\n"
                << "VPPI range       : " << min_pressure_bar << " to +" << max_pressure_bar << " bar\n"
                << "Samples/cycle    : " << samples_per_cycle << '\n'
                << "Cycles           : " << number_of_cycles << '\n'
                << "Update rate      : " << updates_per_second << " commands/sec\n"
                << "Sine frequency   : " << sine_frequency_hz << " Hz\n"
                << "Total updates    : " << total_updates << '\n'
                << "Total duration   : " << test_duration_seconds << " seconds\n"
                << "=================================================\n";
            const auto old_flags = std::cout.flags();
            const auto old_precision = std::cout.precision();

            std::cout << std::fixed << std::setprecision(3);
            const auto start = std::chrono::steady_clock::now();

            // Send 60 commands total:
            //
            // Cycle 1 = samples 0..29
            // Cycle 2 = samples 0..29 again
            for (int i = 0; i < total_updates; ++i) {
                const int sample_index = i % samples_per_cycle;
                const int cycle_number = i / samples_per_cycle + 1;
                const double scheduled_time = static_cast<double>(i) / updates_per_second;

                // Maintain an absolute schedule so timing
                // errors do not accumulate.
                std::this_thread::sleep_until(start + std::chrono::duration<double>(scheduled_time));

                std::cout << "\n--- Cycle " << cycle_number << '/' << number_of_cycles << " | Sample "
                    << sample_index + 1 << '/' << samples_per_cycle << " | t = " << scheduled_time
                    << " s ---\n";

                // Command next sine-wave pressure value.
                status = s826_set_pressure(board, dac_channel, samples[sample_index]);

                if (status != S826_ERR_OK) {
                    std::cerr << "Diagnostic pressure " "command failed.\n";
                    break;
                }

                // Read VPPI feedback approximately
                // 100 ms after each command.
                status = s826_read_pressure(board, actual_voltage, actual_pressure, 100);

                if (status != S826_ERR_OK) {
                    std::cerr << "Diagnostic feedback " "read failed.\n";
                    break;
                }

                const double pressure_error = actual_pressure - samples[sample_index];

                std::cout << "Commanded pressure : " << samples[sample_index] << " bar\n"
                    << "Measured pressure  : " << actual_pressure << " bar\n"
                    << "Pressure error     : " << pressure_error << " bar\n";
            }

            std::cout.flags(old_flags);
            std::cout.precision(old_precision);

            if (status != S826_ERR_OK) {
                // Leave while loop and use common shutdown
                // path, which commands 0 bar.
                break;
            }

            // Hold the final sample until the nominal end of
            // the second complete cycle.
            std::this_thread::sleep_until(start + std::chrono::duration<double>(test_duration_seconds));

            // IMPORTANT:
            // 0 bar on this VPPI corresponds to ~5 V.
            std::cout << "\nEnd of diagnostic. " "Returning to 0 bar...\n";
            status = s826_set_pressure(board, dac_channel, 0.0);

            if (status != S826_ERR_OK) {
                break;
            }

            // Verify neutral pressure.
            status = s826_read_pressure(board, actual_voltage, actual_pressure, 250);

            if (status != S826_ERR_OK) {
                break;
            }

            std::cout << "\n"
                << "============== Diagnostic Complete ==============\n"
                << total_updates << " pressure commands sent.\n"
                << "Two complete sine cycles completed.\n"
                << "Final command: 0 bar (~5 V command).\n"
                << "=================================================\n";
            continue;
        }

        // MANUAL PRESSURE CONTROL
        if (option != 1) {
            std::cerr << "Select 1, 2, or 3.\n";
            continue;
        }

        std::cout << "\nEnter desired pressure in bar (" << min_pressure_bar << " to +" << max_pressure_bar
            << "): ";

        if (!(std::cin >> desired_pressure)) {
            if (!std::cin.eof()) {
                std::cerr << "Invalid pressure input.\n";
                status = -1;
            }

            break;
        }

        // Negative pressures are VALID on this VPPI.
        if (desired_pressure < min_pressure_bar || desired_pressure > max_pressure_bar) {
            std::cerr << "Pressure must be between " << min_pressure_bar << " and " << max_pressure_bar
                << " bar.\n";
            continue;
        }

        // Command pressure
        status = s826_set_pressure(board, dac_channel, desired_pressure);

        if (status != S826_ERR_OK) {
            std::cerr << "Failed to set pressure.\n";
            break;
        }

        // Wait and read pressure feedback
        status = s826_read_pressure(board, actual_voltage, actual_pressure, 250);

        if (status != S826_ERR_OK) {
            std::cerr << "Failed to read pressure.\n";
            break;
        }

        std::cout << "\n"
            << "============== Pressure Status ==============\n"
            << "Commanded pressure : " << desired_pressure << " bar\n"
            << "Actual pressure    : " << actual_pressure << " bar\n"
            << "Actual voltage     : " << actual_voltage << " V\n"
            << "Pressure error     : " << actual_pressure - desired_pressure << " bar\n"
            << "=============================================\n";
    }

    // Shutdown
    std::cout << "\nSetting VPPI pressure to 0 bar...\n";

    // IMPORTANT:
    //
    // Because s826_set_pressure() performs the proper mapping,
    // 0.0 bar here generates ~5 V.
    //
    // DO NOT directly write DAC = 0 here, because DAC = 0
    // means 0 V, which corresponds to -1 bar on this VPPI.
    int shutdown_status = s826_set_pressure(board, dac_channel, 0.0);

    if (shutdown_status != S826_ERR_OK) {
        std::cerr << "Failed to set pressure to zero: " << shutdown_status << '\n';
        status = shutdown_status;
    }

    // Stop ADC conversions.
    const int adc_stop_status = S826_AdcEnableWrite(board, 0);

    if (adc_stop_status != S826_ERR_OK) {
        std::cerr << "Warning: failed to stop ADC: " << adc_stop_status << '\n';
    }

    S826_SystemClose();

    std::cout << "Sensoray system closed.\n";
    return status == S826_ERR_OK ? 0 : 1;
}
