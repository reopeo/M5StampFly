# StampFly

## Framework

Platformio

## Observation UDP configuration

This firmware is an observation-only build: it permanently inhibits arming and
motor output. Before building, copy `secrets.ini.example` to `secrets.ini` and
set the private Wi-Fi credentials and the static PC UDP endpoint. The local
`secrets.ini` is ignored by Git and must not be committed.

## Base on project

[M5Fly-kanazawa/StampFly2024June (github.com)](https://github.com/M5Fly-kanazawa/StampFly2024June)

## Product introduction

[M5Stampfly](https://docs.m5stack.com/en/app/Stamp%20Fly)

## Third-party libraries

fastled/FastLED

tinyu-zhao/INA3221

mathertel/OneButton @ ^2.5.0
