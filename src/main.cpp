#include "826api.h"
#include "Festo.h"
#include "Potentiometer.h"
#include "Sensoray826.h"

#include <iostream>

int main()
{
    Sensoray826 sensoray(0);

    int status = sensoray.initialize();

    if (status != S826_ERR_OK) {
        std::cerr << "Sensoray initialization failed: "
                  << status << '\n';
        return 1;
    }

    // board 0 is owned by the shared Sensoray826 object.
    // Each Festo gets its own DAC channel, ADC channel, and ADC slot.
    Festo festo1(sensoray, 0, 15, 0);
    Festo festo2(sensoray, 1, 14, 1);
    Festo festo3(sensoray, 2, 13, 2);

    // Potentiometer uses AIN12 and ADC slot 3.
    Potentiometer pot1(sensoray, 12, 3);

    Festo* festos[] = {&festo1, &festo2, &festo3};

    for (int i = 0; i < 3; ++i) {
        status = festos[i]->initialize();

        if (status != S826_ERR_OK) {
            std::cerr << "Festo " << i + 1
                      << " initialization failed: "
                      << status << '\n';
            sensoray.close();
            return 1;
        }
    }

    status = pot1.initialize();

    if (status != S826_ERR_OK) {
        std::cerr << "Potentiometer initialization failed: "
                  << status << '\n';
        sensoray.close();
        return 1;
    }

    status = sensoray.startAdc();

    if (status != S826_ERR_OK) {
        std::cerr << "Failed to start Sensoray ADC: "
                  << status << '\n';
        sensoray.close();
        return 1;
    }

    while (true) {
        std::cout << "\n"
                  << "================ Controller ================\n"
                  << "1. Set Festo 1 pressure\n"
                  << "2. Set Festo 2 pressure\n"
                  << "3. Set Festo 3 pressure\n"
                  << "4. Read all Festo pressures\n"
                  << "5. Read potentiometer\n"
                  << "6. Run Festo 1 diagnostic\n"
                  << "7. Run Festo 2 diagnostic\n"
                  << "8. Run Festo 3 diagnostic\n"
                  << "9. Quit\n"
                  << "============================================\n"
                  << "Select option: ";

        int option = 0;

        if (!(std::cin >> option)) {
            std::cerr << "Invalid input.\n";
            break;
        }

        if (option == 9) break;

        if (option >= 1 && option <= 3) {
            double desired_pressure_kpa = 0.0;

            std::cout
                << "Enter desired pressure (-100 to +100 kPa): ";

            if (!(std::cin >> desired_pressure_kpa)) {
                std::cerr << "Invalid pressure input.\n";
                break;
            }

            Festo& selected_festo =
                *festos[option - 1];

            status =
                selected_festo.setPressure(
                    desired_pressure_kpa
                );

            if (status != S826_ERR_OK) continue;

            double voltage = 0.0;
            double actual_pressure_kpa = 0.0;

            status = selected_festo.readPressure(
                voltage,
                actual_pressure_kpa,
                250
            );

            if (status == S826_ERR_OK) {
                std::cout
                    << "Measured pressure: "
                    << actual_pressure_kpa
                    << " kPa\n"
                    << "Pressure error: "
                    << actual_pressure_kpa -
                       desired_pressure_kpa
                    << " kPa\n";
            }

            continue;
        }

        if (option == 4) {
            for (int i = 0; i < 3; ++i) {
                double voltage = 0.0;
                double pressure_kpa = 0.0;

                std::cout << "\nFesto "
                          << i + 1 << ":\n";

                status = festos[i]->readPressure(
                    voltage,
                    pressure_kpa,
                    0
                );

                if (status != S826_ERR_OK) {
                    std::cerr
                        << "Failed to read Festo "
                        << i + 1 << ".\n";
                }
            }

            continue;
        }

        if (option == 5) {
            double voltage = 0.0;

            status = pot1.readVoltage(voltage);

            if (status == S826_ERR_OK) {
                std::cout
                    << "Pot voltage: "
                    << voltage << " V\n"
                    << "Pot position: "
                    << pot1.readPercent(voltage)
                    << "%\n";
            }

            continue;
        }

        if (option >= 6 && option <= 8) {
            Festo& selected_festo =
                *festos[option - 6];

            status =
                selected_festo.runDiagnostic();

            if (status != S826_ERR_OK) {
                std::cerr
                    << "Diagnostic failed: "
                    << status << '\n';
            }

            continue;
        }

        std::cerr << "Select a valid option.\n";
    }

    std::cout
        << "\nReturning all Festos to 0 kPa...\n";

    for (Festo* festo : festos) {
        const int shutdown_status =
            festo->setPressure(0.0);

        if (shutdown_status != S826_ERR_OK) {
            std::cerr
                << "Warning: failed to return a Festo "
                << "to 0 kPa. Error: "
                << shutdown_status << '\n';
        }
    }

    const int close_status = sensoray.close();

    if (close_status != S826_ERR_OK) {
        std::cerr << "Sensoray close returned error: "
                  << close_status << '\n';
        return 1;
    }

    std::cout << "Sensoray system closed.\n";
    return 0;
}
