# Sensoray 826 Pressure Controller

C++ terminal application for commanding a VPPI pressure regulator through a Sensoray 826 board. Supports manual pressure commands, analog pressure feedback, and a timed sine-wave diagnostic.

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
| Pressure command | 0–6 bar mapped to 0–10 V |
| DAC channel | Selectable, 0–7; default is DAC0 |
| Pressure feedback | AIN15, assigned to ADC slot 0 |
| ADC input range | ±10 V |
| Feedback conversion | 0–10 V interpreted as 0–6 bar |

Custom initialization changes the board, DAC channel, and initial pressure. The feedback channel remains AIN15, as configured in `s826_init_vppi_adc()`.

## Using the program

Initialization runs before the operating menu. Enter:

- `default`: use the first detected board, DAC channel 0, and an initial command of 0 bar.
- Any other word, such as `custom`: enter a detected board number (0–15), DAC channel (0–7), and initial pressure (0–6 bar).

The program configures the DAC, applies the initial pressure, starts ADC conversion, and reads feedback once. After initialization succeeds, it displays:

```text
1. Set pressure manually
2. Diagnostic (0-6 bar sine wave, 10 seconds, 3 updates/sec)
3. Quit
```

### Manual pressure

Choose `1` and enter a pressure from 0 to 6 bar. The program writes the command and reads feedback, then displays commanded pressure, measured pressure, and measured voltage before returning to the menu. Entering `-1` at the pressure prompt exits the program.

### Diagnostic

Choose `2` to run the diagnostic on the initialized board and DAC channel.

| Parameter | Value |
| --- | --- |
| Waveform | `P[i] = 3 + 3 * sin(2π * i / 30)` bar, for `i = 0…29` |
| Nominal pressure range | 0–6 bar |
| Starting command | 3 bar |
| Samples | 30 across one sine cycle |
| Command update rate | Approximately 3 per second |
| Test duration | 10 seconds |
| Sine frequency | 0.1 Hz |

Three updates per second means one command about every 333 ms. The sine cycle takes 10 seconds; this is not a 3 Hz pressure oscillation.

Output includes a test summary, numbered samples, scheduled times, command pressures, command voltages, and DAC setpoints. These are commanded values; the diagnostic does not read pressure feedback during the test. Timing uses `steady_clock` and `sleep_until`; actual command times may vary with system scheduling and I/O.

At the end, the program commands 0 bar and returns to the menu. The menu does not accept another selection while the diagnostic runs.

### Quit

Choose `3` to attempt a 0 bar command and close the Sensoray system. Command or feedback errors also leave the operating loop through this shutdown path. Forced termination does not run that cleanup.

## Project layout

| File | Purpose |
| --- | --- |
| `src/main.cpp` | Initialization call, operating menu, diagnostic timing, and shutdown |
| `src/pressure_controller.cpp` | Hardware initialization, sine sample generation, pressure conversion, DAC writes, and ADC reads |
| `include/pressure_controller.h` | Controller declarations and shared maximum pressure constant |
| `include/826api.h`, `include/826const.h` | Sensoray SDK headers |
| `CMakeLists.txt` | Build configuration and middleware library lookup |

## Troubleshooting

- **Program appears unchanged:** save your files, rebuild, and launch `./build/sensoray826_pressure_control`. An already running process continues using its previous code.
- **No operating menu:** complete initialization first. If it fails, inspect the printed error. ADC reads currently wait indefinitely, so missing feedback conversion can also prevent initialization from finishing.
- **Library not found during configuration:** set `SENSORAY826_LIB` to the installed middleware library's full path.
- **No boards found or system-open error:** check the board connection, driver installation, and device access.

The project has been built successfully in the development environment. Hardware behavior has not been verified by automated testing.
