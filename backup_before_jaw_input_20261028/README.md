# Sensoray 826 Pressure Controller

C++17 terminal application for controlling three FESTO VPPI proportional pressure regulators and reading one potentiometer through a shared Sensoray 826 DAQ.

The software uses one shared `Sensoray826` object for board-level initialization and I/O. Each `Festo` and `Potentiometer` object stores only the channels and conversion information specific to that device.

## Pressure units

All pressure values in the application are expressed in **kilopascals (kPa)**.

The analog conversion assumes the FESTO pressure limits match the configured pressure range. The current software defaults to -50 to +50 kPa. **Analog feedback scaling is not verified against the FESTO display and must not be treated as calibrated physical pressure.**

For the current software range (0–5 V command):

| Pressure | Command / feedback voltage |
| --- | --- |
| -50 kPa | 0 V |
| 0 kPa | 2.5 V |
| +50 kPa | 5 V |

The configured range is -0.5 to +0.5 bar, since 1 bar = 100 kPa.

## Current channel mapping

| Device | DAC output | ADC channel | ADC slot |
| --- | ---: | ---: | ---: |
| Festo 1 | DAC0 | AIN15 | 0 |
| Festo 2 | DAC1 | AIN14 | 1 |
| Festo 3 | DAC2 | AIN13 | 2 |
| Potentiometer | - | AIN12 | 3 |

The three FESTOs use separate DAC command outputs and separate analog feedback inputs. ADC slots are also unique so the four devices can coexist in the Sensoray scan list.

## Class responsibilities

### Sensoray826

`Sensoray826` owns shared board-level behavior:

- opens and closes the Sensoray system
- verifies the selected board exists
- configures DAC channels
- configures and enables ADC slots
- starts/stops ADC conversions
- performs low-level DAC writes
- performs low-level ADC voltage reads

Only one `Sensoray826` object is created for board 0.

### Festo

Each `Festo` object stores:

- DAC channel
- ADC feedback channel
- ADC slot
- pressure range in kPa
- analog voltage range

The class handles:

- kPa-to-voltage conversion
- voltage-to-kPa conversion
- DAC setpoint conversion
- pressure commands
- feedback reads
- sine-wave diagnostic generation/execution

The default pressure range is -50 to +50 kPa.

`setPressure(pressure_kpa)` retains the terminal application's successful-write
messages. Time-sensitive adapters can call `setPressure(pressure_kpa, false)` to
omit those messages from the synchronous DAC write path. Pressure conversion,
DAC values, validation, and error returns are identical; validation and DAC
errors still print to stderr. This option does not alter any caller's TTL or
acknowledgment checks.

### Potentiometer

The `Potentiometer` class stores:

- ADC channel
- ADC slot
- minimum voltage
- maximum voltage

It can read the potentiometer voltage and convert that voltage to a percentage of its configured range.

## Initialization order

The intended startup sequence is:

1. Create one `Sensoray826` object.
2. Open the Sensoray system once.
3. Create all three `Festo` objects and the `Potentiometer` object.
4. Initialize each device so its DAC/ADC channels and slots are configured.
5. Start ADC conversion once after all slots have been configured.
6. Enter the user menu.
7. On shutdown, command all FESTOs to 0 kPa and close the Sensoray system once.

This avoids each device independently opening, closing, or globally reconfiguring the Sensoray board.

## Diagnostic

The current diagnostic preserves the previous behavior:

- 30 pressure samples per sine cycle
- 2 cycles
- 10 command updates per second
- pressure range from -50 to +50 kPa
- 100 ms feedback delay after each pressure command

Because there are 30 samples per cycle and 10 updates per second, the resulting sine-wave frequency is approximately 0.333 Hz. The update rate is not the same thing as the sine-wave frequency.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
./build/sensoray826_pressure_control
```

From inside an already-created `build/` directory:

```bash
cmake ..
cmake --build .
./sensoray826_pressure_control
```

If an old `CMakeCache.txt` points to another computer or source directory, delete and recreate the build directory:

```bash
cd ..
rm -rf build
cmake -S . -B build
cmake --build build
```

## Offline tests

Offline conversion and logging checks use fake `Sensoray826` methods and do not
link the Sensoray SDK or open a board:

```bash
cmake -S . -B build_quiet_fix -DCMAKE_INSTALL_PREFIX="$PWD/install_quiet_fix" -DBUILD_TESTING=ON
cmake --build build_quiet_fix -j2
ctest --test-dir build_quiet_fix --output-on-failure
cmake --install build_quiet_fix
```

The build also compiles the standalone application; the test command runs only
the fake-I/O test executable.

## Project layout

```text
include/
    826api.h
    826const.h
    Sensoray826.h
    Festo.h
    Potentiometer.h

src/
    sensoray826.cpp
    festo.cpp
    potentiometer.cpp
    main.cpp
```

## Notes

- The Sensoray ADC is currently configured for the +/-10 V range.
- The potentiometer defaults to a 0-5 V useful signal range.
- All configured ADC slots are enabled with `S826_BITSET`, so one device does not disable another device's slot.
- The program returns all FESTOs to 0 kPa before closing the Sensoray system during normal shutdown.
- Hardware behavior still needs to be verified on the physical system.

## Provisional empirical feedback mapping

The three FESTOs now use 0–5 V analog input/output for −50 to +50 kPa, while the Sensoray DAC hardware retains its 0–10 V span. Outgoing commands are unchanged.

A previous full-scale feedback measurement was 8.96 V with the FESTO configured for 0–10 V. As a **provisional** measurement-based compensation, the feedback-only conversion uses a gain of 0.896. Under a 0–5 V output configuration, the extrapolated positive-full-scale feedback voltage is 4.48 V:

```
estimated_kpa = -50.0 + 100.0 * measured_feedback_volts / 4.48
```

This is NOT a verified pressure calibration, and may differ for individual devices or operating conditions. Verify against independent pressure instrumentation before closed-loop or safety-critical use. Raw feedback voltage is preserved in terminal output.
