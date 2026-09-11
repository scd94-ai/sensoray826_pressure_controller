
#pragma once

inline constexpr double max_pressure_bar = 6.0;

// Generate one sine cycle of pressure samples in the range 0-max_pressure_bar.
int s826_diagnostic(double* buf, int resolution);

unsigned int pressure_to_dac(double pressure_bar);

int s826_set_pressure(
    unsigned int board,
    unsigned int channel,
    double pressure_bar);

int s826_init_vppi_adc(unsigned int board);

int s826_read_pressure(
    unsigned int board,
    double& voltage,
    double& pressure_bar);

int s826_init(unsigned int& board, unsigned int& dac_channel);
