#include "Festo.h"
#include "Potentiometer.h"
#include "826api.h"

#include <iostream>

int main()
{
    Festo festo1(0, 0, 15, 0);
    Festo festo2(0, 1, 14, 1);
    Festo festo3(0, 2, 13, 2);

    Potentiometer pot1(0, 12, 3);

    int status1 = festo1.initialize();
    if (status1 != S826_ERR_OK) {
        std::cerr << "Festo 1 initialization failed: " << status1 << '\n';
        S826_SystemClose();
        return 1;
    }

    int status2 = festo2.initialize();
    if (status2 != S826_ERR_OK) {
        std::cerr << "Festo 2 initialization failed: " << status2 << '\n';
        S826_SystemClose();
        return 1;
    }

    int status3 = festo3.initialize();
    if (status3 != S826_ERR_OK) {
        std::cerr << "Festo 3 initialization failed: " << status3 << '\n';
        S826_SystemClose();
        return 1;
    }

    int pot_status = pot1.initialize();
    if (pot_status != S826_ERR_OK) {
        std::cerr << "Potentiometer initialization failed: "
                  << pot_status << '\n';
        S826_SystemClose();
        return 1;
    }

    while (true) {
        std::cout << "\n"
                  << "============== Controller ==============\n"
                  << "1. Test Festo 1\n"
                  << "2. Test Festo 2\n"
                  << "3. Test Festo 3\n"
                  << "4. Read all Festo pressures\n"
                  << "5. Read potentiometer\n"
                  << "6. Run Festo 1 diagnostic\n"
                  << "7. Run Festo 2 diagnostic\n"
                  << "8. Run Festo 3 diagnostic\n"
                  << "9. Quit\n"
                  << "========================================\n"
                  << "Select option: ";

        int option;
        std::cin >> option;

        if (!std::cin) {
            std::cerr << "Invalid input.\n";
            break;
        }

        if (option == 9) {
            break;
        }

        if (option >= 1 && option <= 3) {
            double desired_pressure;

            std::cout << "Enter desired pressure (-1 to +1 bar): ";
            std::cin >> desired_pressure;

            Festo* selected_festo = nullptr;

            if (option == 1) selected_festo = &festo1;
            if (option == 2) selected_festo = &festo2;
            if (option == 3) selected_festo = &festo3;

            int status = selected_festo->setPressure(desired_pressure);

            if (status != S826_ERR_OK) {
                continue;
            }

            double voltage = 0.0;
            double pressure = 0.0;

            status = selected_festo->readPressure(
                voltage,
                pressure,
                250
            );

            if (status == S826_ERR_OK) {
                std::cout << "Measured pressure: "
                          << pressure << " bar\n";
            }

            continue;
        }

        if (option == 4) {
            double voltage;
            double pressure;

            std::cout << "\nFesto 1:\n";
            festo1.readPressure(voltage, pressure, 0);

            std::cout << "\nFesto 2:\n";
            festo2.readPressure(voltage, pressure, 0);

            std::cout << "\nFesto 3:\n";
            festo3.readPressure(voltage, pressure, 0);

            continue;
        }

        if (option == 5) {
            double voltage = 0.0;

            if (pot1.readVoltage(voltage) == S826_ERR_OK) {
                std::cout << "Pot voltage: "
                          << voltage << " V\n"
                          << "Pot position: "
                          << pot1.readPercent(voltage)
                          << "%\n";
            }

            continue;
        }

        if (option == 6) {
            festo1.runDiagnostic();
            continue;
        }

        if (option == 7) {
            festo2.runDiagnostic();
            continue;
        }

        if (option == 8) {
            festo3.runDiagnostic();
            continue;
        }

        std::cerr << "Select a valid option.\n";
    }

    // Return all regulators to 0 bar
    festo1.setPressure(0.0);
    festo2.setPressure(0.0);
    festo3.setPressure(0.0);

    S826_SystemClose();

    std::cout << "Sensoray system closed.\n";

    return 0;
}