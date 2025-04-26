# ESP-IDF PCA9685 Library

A lightweight PCA9685 PWM controller library for ESP-IDF, using native I2C driver.

## Features
- Initialize PCA9685 with custom I2C port, address, and GPIOs.
- Set PWM frequency (24-1526 Hz).
- Set duty cycle (0-4095, 12-bit) for MOSFETs or LEDs.
- Set servo pulse width (500-2500 µs) for servo control.
- No external dependencies beyond ESP-IDF.

## Installation
```bash
git clone https://github.com/yourusername/esp-idf-pca9685.git


Add to components/ and include in CMakeLists.txt.

## Usage

#include "pca9685.h"

pca9685_dev_t dev;
pca9685_init(&dev, I2C_NUM_0, 0x40, 8, 9, 100000);
pca9685_set_frequency(&dev, 50.0f);
pca9685_set_servo_pulse(&dev, PCA9685_CHANNEL_0, 1500);
pca9685_set_duty(&dev, PCA9685_CHANNEL_4, 2048);
