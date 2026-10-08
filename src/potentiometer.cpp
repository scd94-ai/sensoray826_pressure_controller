#include "Potentiometer.h"
#include <iostream>

Potentiometer::Potentiometer(
    unsigned int board_num,
    unsigned int adc_channel,
    unsigned int adc_slot,
    double min_voltage,
    double max_voltage
)   
    : board_num_(board_num),
      adc_channel_(adc_channel),
      adc_slot_(adc_slot),
      min_voltage_(min_voltage),
      max_voltage_(max_voltage)
{     
}

int Potentiometer::initializeADC(){
    return 1;
}

int Potentiometer::initialize(){
    return 1;
}

int Potentiometer::readVoltage(double& voltage){
    return 1;
}

double readPercent(double voltage){
    return 1.0;
}