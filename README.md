# Sensoray 826 Pressure Controller

C++17 terminal application for controlling three FESTO VPPI proportional pressure regulators and reading one potentiometer through a shared Sensoray 826 DAQ.

The software uses one shared `Sensoray826` object for board-level initialization and I/O. Each `Festo` and `Potentiometer` object stores only the channels and conversion information specific to that device.

## Pressure units

All pressure values in the application are expressed in **kilopascals (kPa)**.

The current VPPI configuration is:

| Pressure | Command / feedback voltage |
| --- | --- |
| -100 kPa | 0 V |
| 0 kPa | 5 V |
| +100 kPa | 10 V |

This is the same physical range as -1 to +1 bar, since 1 bar = 100 kPa.

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

The default pressure range is -100 to +100 kPa.

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
- pressure range from -100 to +100 kPa
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

## Project layout

```text
include/
    826api.h
    826const.h
    Sensoray826.h
    Festo.h
    Potentiometer.h

src/
    Sensoray826.cpp
    Festo.cpp
    potentiometer.cpp
    main.cpp
```

## Notes

- The Sensoray ADC is currently configured for the +/-10 V range.
- The potentiometer defaults to a 0-5 V useful signal range.
- All configured ADC slots are enabled with `S826_BITSET`, so one device does not disable another device's slot.
- The program returns all FESTOs to 0 kPa before closing the Sensoray system during normal shutdown.
- Hardware behavior still needs to be verified on the physical system.
