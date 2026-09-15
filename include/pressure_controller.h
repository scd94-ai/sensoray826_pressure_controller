#ifndef PRESSURE_CONTROLLER_H
#define PRESSURE_CONTROLLER_H

// Exact VPPI model:
// VPPI-5L-3-G18-1V1H-V1-S1D
//
// Pressure range: -1 to +1 bar
// Analog setpoint: 0 to 10 V
inline constexpr double min_pressure_bar = -1.0;
inline constexpr double max_pressure_bar = 1.0;
inline constexpr double min_command_voltage = 0.0;
inline constexpr double max_command_voltage = 10.0;

int s826_diagnostic(double* buf, int resolution);
unsigned int pressure_to_dac(double pressure_bar);
int s826_set_pressure(unsigned int board, unsigned int channel, double pressure_bar);
int s826_init_vppi_adc(unsigned int board);
int s826_read_pressure(unsigned int board, double& voltage, double& pressure_bar, unsigned int response_wait_ms = 100);
int s826_init(unsigned int& board, unsigned int& dac_channel);

#endif
