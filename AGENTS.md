# Repository Guidelines

## Project Structure & Module Organization

This repository contains PlatformIO firmware for the M5StampFly, targeting an ESP32-S3 with the Arduino framework. Application code lives in `src/`; `main.cpp` provides Arduino's `setup()` and `loop()`, while paired `.cpp`/`.hpp` files separate flight control, sensors, telemetry, LEDs, and other hardware concerns. Third-party or vendor code is under `lib/` (notably BMI270, VL53L3C, and Madgwick AHRS); avoid style-only changes there. `include/` is available for project-wide headers. Prebuilt release images live in `firmware/`. Build configuration is in `platformio.ini`, and CI definitions are in `.github/workflows/`.

## Build, Test, and Development Commands

- `pio run` builds the `esp32-s3-devkitc-1` release environment and is the primary CI check.
- `pio run -t upload` builds and flashes a connected board.
- `pio device monitor -b 115200` opens the serial monitor at the configured baud rate.
- `clang-format -i src/*.{cpp,hpp,h}` formats project sources using `.clang-format` (clang-format 13 in CI).
- `pio run -t clean` removes generated PlatformIO build artifacts.

Install PlatformIO Core before running these commands. Dependencies declared in `platformio.ini` are fetched automatically.

## Coding Style & Naming Conventions

Use C++/Arduino conventions already present in `src/`: four-space indentation, attached braces, no tabs, and a 120-column limit. Keep implementation/header pairs named with lowercase snake case, such as `flight_control.cpp` and `flight_control.hpp`. Functions and variables use `snake_case`; constants and macros use uppercase names where appropriate. Include the matching project header in each implementation and keep hardware-specific logic within its module. Run clang-format before committing; Arduino Lint and formatting checks run in CI.

## Testing Guidelines

There is currently no dedicated unit-test directory or coverage requirement. Every change must at least pass `pio run`. For hardware-facing changes, test on an M5StampFly and describe the board, observed behavior, and serial output in the pull request. Add focused regression tests if introducing testable logic or a PlatformIO test environment.

## Commit & Pull Request Guidelines

Recent history favors short, imperative subjects, sometimes scoped (for example, `[lib][vl53l3c] fix include path`). Keep each commit focused and state the affected area when useful. Pull requests should explain the change and motivation, list build and hardware validation, link related issues, and include logs, screenshots, or video when behavior is visible. Do not commit `.pio/` or other generated build artifacts; add firmware binaries only for an intentional release.

## Integrated Drone VIO Role

This repository is the flight-controller component of the parent Drone VIO
workspace. Own flight safety, sensors, AHRS, attitude/altitude control, motor
mixing, RC reception, and telemetry. Do not edit the camera or ROS components
from this repository.

Read the parent `../specs/` documents before changing an external interface.
Packet layouts, units, coordinate frames, timing, arming, failsafe behavior,
control rates, and PC-to-flight authority are cross-component contracts. Do not
implement an unresolved `TBD` or make a breaking contract change locally; return
a proposal to the primary coordinator first.

Treat changes to motor output, arming, failsafe, and control timing as
safety-critical. Firmware build results are not hardware evidence. Flashing and
motor/flight tests require explicit authorization. Observation and calibration
bench tests keep motor output at zero. A motor-running test with installed
propellers is not authorized by a software output cap alone; it requires
explicit approval plus a restraint, guarding, output limit, e-stop, and abort
plan.
