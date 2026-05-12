# AlarmClockRestoMod

Portable reference implementation for the ESP32-S3 alarm clock resto-mod.

## Included in this repository

This repository now contains a small C++ control core and a lightweight web UI that cover the requested behavior:

- 7-segment display modes for current time, next alarm, sleep timer, and IP address
- alarm scheduling with day-of-week selection, snooze, vacation mode, and radio/buzzer alarm types
- microSD-style configuration files for:
  - internet radio stations
  - playlists
  - alarms
- radio source selection and tuner-based station selection
- access-point and Wi-Fi settings editable from the web UI
- Bluetooth state handling for speaker pairing and phone discovery with 1 minute timeout
- sleep mode with configurable default duration

The implementation is written as a portable reference so it can be built and tested in this repository without requiring ESP32-specific toolchains.

## Build

```bash
cmake -S /home/runner/work/AlarmClockRestoMod/AlarmClockRestoMod -B /home/runner/work/AlarmClockRestoMod/AlarmClockRestoMod/build
cmake --build /home/runner/work/AlarmClockRestoMod/AlarmClockRestoMod/build
```

## Test

```bash
ctest --test-dir /home/runner/work/AlarmClockRestoMod/AlarmClockRestoMod/build --output-on-failure
```

## Run the web interface

```bash
/home/runner/work/AlarmClockRestoMod/AlarmClockRestoMod/build/alarm_clock
```

Then open `http://127.0.0.1:8080/`.

## Storage layout

The application persists editable configuration in a `storage/` directory to mirror the microSD-card-based setup:

- `storage/system.cfg`
- `storage/stations.cfg`
- `storage/playlists.cfg`
- `storage/alarms.cfg`

## Web configuration

The control panel allows you to:

- edit access point and Wi-Fi settings
- toggle auto-dim and vacation mode
- choose playlist mode or internet-radio mode
- add radio stations mapped to tuner values
- add playlists stored on the microSD card
- add alarms with time, days, alarm type, source name, enabled flag, and sleep duration
- trigger radio, sleep, Bluetooth, and IP-display actions

## Notes for ESP32-S3 integration

This code focuses on the configuration, scheduling, and state-machine logic. Hardware-specific integration points for TM1637, DS3231, MPR121, MAX98357A, Wi-Fi, and Bluetooth can be wired to the controller state in `src/alarm_clock.cpp`.
