#include "Festo.h"
#include "826api.h"

#include <iostream>

int main()
{
    Festo festo;

    int status = festo.initialize();

    if (status != S826_ERR_OK) {
        std::cerr << "Initialization failed with error: " << status << '\n';
        S826_SystemClose();
        return 1;
    }

    double desired_pressure = 0.0;
    double actual_voltage = 0.0;
    double actual_pressure = 0.0;

    while (true) {
        std::cout << "\n"
                  << "================ VPPI Controller ================\n"
                  << "1. Set pressure manually\n"
                  << "2. Run diagnostic\n"
                  << "3. Quit\n"
                  << "=================================================\n"
                  << "Select option: ";

        int option = 0;

        if (!(std::cin >> option)) {
            std::cerr << "Invalid input.\n";
            break;
        }

        // Quit
        if (option == 3) {
            break;
        }

        // Diagnostic
        if (option == 2) {
            status = festo.runDiagnostic();

            if (status != S826_ERR_OK) {
                std::cerr << "Diagnostic failed with error: "
                          << status << '\n';
                break;
            }

            continue;
        }

        // Manual pressure
        if (option == 1) {
            std::cout << "Enter desired pressure (-1 to +1 bar): ";

            if (!(std::cin >> desired_pressure)) {
                std::cerr << "Invalid pressure input.\n";
                break;
            }

            status = festo.setPressure(desired_pressure);

            if (status != S826_ERR_OK) {
                continue;
            }

            status = festo.readPressure(
                actual_voltage,
                actual_pressure,
                250
            );

            if (status != S826_ERR_OK) {
                std::cerr << "Failed to read pressure.\n";
                break;
            }

            std::cout << "\n"
                      << "============== Pressure Status ==============\n"
                      << "Commanded pressure : " << desired_pressure << " bar\n"
                      << "Actual pressure    : " << actual_pressure << " bar\n"
                      << "Actual voltage     : " << actual_voltage << " V\n"
                      << "Pressure error     : "
                      << actual_pressure - desired_pressure << " bar\n"
                      << "=============================================\n";

            continue;
        }

        std::cerr << "Select 1, 2, or 3.\n";
    }

    // Return regulator to neutral pressure before shutdown
    std::cout << "\nSetting pressure to 0 bar...\n";

    int shutdown_status = festo.setPressure(0.0);

    if (shutdown_status != S826_ERR_OK) {
        std::cerr << "Failed to set pressure to 0 bar.\n";
        status = shutdown_status;
    }

    S826_SystemClose();

    std::cout << "Sensoray system closed.\n";

    return status == S826_ERR_OK ? 0 : 1;
}