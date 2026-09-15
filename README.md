# Sensoray 826 Pressure Controller

C++ terminal application for commanding a VPPI-5L-3-G18-1V1H-V1-S1D pressure regulator through a Sensoray 826 board. Supports manual pressure commands from −1 to +1 bar, analog pressure feedback, and a timed sine-wave diagnostic. Feedback is displayed for monitoring; the application does not automatically adjust commands to correct pressure error.

## Requirements

- Linux with the Sensoray 826 driver installed and the board accessible.
- Sensoray 826 SDK middleware library (`lib826_64`).
- CMake 3.16 or newer and a C++17 compiler.

The Sensoray API headers are included in `include/`. CMake searches for the middleware library in:

```text
$HOME/Downloads/sdk_826_linux_3.3.17/middleware
```

## Build and run

From the project root:

```bash
cmake -S . -B build
cmake --build build
./build/sensoray826_pressure_control
```

If the middleware library is installed elsewhere, provide its full path during configuration:

```bash
cmake -S . -B build -DSENSORAY826_LIB=/path/to/lib826_64.so
```

After editing and saving source or header files, rebuild and restart the program:

```bash
cmake --build build
./build/sensoray826_pressure_control
```

For a Makefile build, you can also run `make` from inside `build/`. Only affected files are recompiled. You do not need to run `cmake ..` for every source edit.

## Hardware configuration

The current code assumes the following signal mapping. Select the DAC channel that matches the regulator's command wiring.

| Setting | Current configuration |
| --- | --- |
| Pressure command | −1 to +1 bar mapped to 0–10 V |
| DAC channel | Selectable, 0–7; default is DAC0 |
| Pressure feedback | AIN15, assigned to ADC slot 0 |
| ADC input range | ±10 V |
| Feedback conversion | 0–10 V interpreted as −1 to +1 bar |

The command and feedback mappings in the code are:

| Pressure | Command / feedback voltage | DAC setpoint |
| --- | --- | --- |
| −1 bar | 0 V | 0 |
| 0 bar | 5 V | 32768 |
| +1 bar | 10 V | 65535 |

Zero pressure therefore requires approximately 5 V. A zero DAC setpoint commands −1 bar.

Custom initialization changes the board, DAC channel, and initial pressure. The feedback channel remains AIN15, as configured in `s826_init_vppi_adc()`.

## Using the program

Initialization runs before the operating menu. Enter:

- `default`: use the first detected board, DAC channel 0, and an initial command of 0 bar.
- Any other word, such as `custom`: enter a detected board number (0–15), DAC channel (0–7), and initial pressure (−1 to +1 bar).

The program configures the DAC, applies the initial pressure, starts ADC conversion, and reads feedback after a 250 ms response delay. After initialization succeeds, it displays:

```text
1. Set pressure manually
2. Diagnostic (-1 to +1 bar sine, 2 cycles, 3 updates/sec)
3. Quit
```

### Manual pressure

Choose `1` and enter a pressure from −1 to +1 bar. The program writes the command, discards pending ADC feedback, waits 250 ms for the regulator to respond, and reads feedback again. It then displays commanded pressure, measured pressure, measured voltage, and pressure error (`measured − commanded`) before returning to the menu.

Negative pressures, including `-1`, are valid commands. Out-of-range pressures return to the menu without issuing a command. Use menu option `3` to quit.

### Diagnostic

Choose `2` to run the diagnostic on the initialized board and DAC channel.

| Parameter | Value |
| --- | --- |
| Waveform | `P[i] = sin(2π * i / 30)` bar, for `i = 0…29`, repeated twice |
| Nominal pressure range | −1 to +1 bar |
| Starting command | 0 bar (approximately 5 V) |
| Samples | 30 per cycle; 60 waveform commands across two cycles |
| Command update rate | Approximately 3 per second |
| Test duration | 20 seconds nominal, followed by a 0 bar command and feedback read |
| Sine frequency | 0.1 Hz |
| Feedback response delay | 100 ms after each waveform command |

Three updates per second means one command about every 333 ms. The sine cycle takes 10 seconds; this is not a 3 Hz pressure oscillation.

Output includes a test summary, cycle and sample numbers, scheduled times, command pressures, command voltages, DAC setpoints, raw ADC values, measured voltages and pressures, and pressure errors (`measured − commanded`). Timing uses `steady_clock` and `sleep_until` with an absolute schedule; actual command times may vary with system scheduling, I/O, and feedback-read delays.

At the end, the program commands 0 bar (approximately 5 V), reads feedback after a 250 ms response delay, and returns to the menu. The menu does not accept another selection while the diagnostic runs.

### Quit

Choose `3` to attempt a 0 bar command (approximately 5 V), stop ADC conversions, and close the Sensoray system. Command or feedback errors during operation, invalid nonnumeric menu or pressure input, and end-of-input also leave the operating loop through this shutdown path. Initialization failures close the Sensoray system and exit without running the common 0 bar shutdown path. Forced termination does not run that cleanup.

## Project layout

| File | Purpose |
| --- | --- |
| `src/main.cpp` | Initialization call, operating menu, diagnostic timing, and shutdown |
| `src/pressure_controller.cpp` | Hardware initialization, sine sample generation, pressure conversion, DAC writes, and ADC reads |
| `include/pressure_controller.h` | Controller declarations and shared pressure and command-voltage limits |
| `include/826api.h`, `include/826const.h` | Sensoray SDK headers |
| `CMakeLists.txt` | Build configuration and middleware library lookup |

## Troubleshooting

- **Program appears unchanged:** save your files, rebuild, and launch `./build/sensoray826_pressure_control`. An already running process continues using its previous code.
- **No operating menu:** complete initialization first. If it fails, inspect the printed error. Feedback reads use a 250 ms response delay during initialization and manual operation, or 100 ms during the waveform, followed by a bounded ADC wait; missing conversions produce an error. The response delay does not guarantee that pressure has fully settled.
- **Unexpected pressure feedback:** check the AIN15 feedback connection and the configured 0–10 V to −1 to +1 bar mapping. The program prints a warning if measured voltage is below −0.1 V or above 10.1 V; it does not clamp the displayed pressure or stop solely because of this warning.
- **Library not found during configuration:** set `SENSORAY826_LIB` to the installed middleware library's full path.
- **No boards found or system-open error:** check the board connection, driver installation, and device access.

Hardware behavior has not been verified by automated testing.
