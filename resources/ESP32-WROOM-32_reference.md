# ESP32-WROOM-32 Reference

## Manufacturer

Espressif Systems

## Project relevance

The ESP32_MOS_X4 board used by H2SSensor is based on an ESP32-WROOM-32-class module. The H2SSensor PlatformIO environment currently targets the generic ESP32 Dev Module profile while the physical board pin mapping is being validated.

## Key module data

- ESP32 dual-core Xtensa LX6 MCU, up to 240 MHz
- 4 MB SPI flash on the classic ESP32-WROOM-32 module
- 3.0 to 3.6 V recommended module supply range
- Integrated 2.4 GHz Wi-Fi and Bluetooth
- GPIO matrix supporting I2C, UART, SPI, PWM and other peripherals
- GPIO21 and GPIO22 are valid general-purpose pins used by this project for the shared I2C bus
- GPIO16, 17, 18, 19, 23, 25, 32 and 33 are available module GPIOs and are used by the current H2SSensor design subject to physical ESP32_MOS_X4 board validation

## Important boot / pin considerations

ESP32 strapping pins must be considered when assigning external circuitry. The current H2SSensor design intentionally avoids using the principal boot-mode strap GPIO0 for normal controls.

## Official documentation

ESP32-WROOM-32 datasheet v3.7:
https://espressif.com/documentation/esp32-wroom-32_datasheet_en.pdf

Espressif technical documents page:
https://www.espressif.com/en/support/documents/technical-documents/esp32-wroom-32-datasheet

The ESP32-WROOM-32 is marked by Espressif as Not Recommended for New Designs (NRND), but it remains appropriate documentation for existing boards using that module.
