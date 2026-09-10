# SSD1306 OLED Controller Reference

## Manufacturer

Solomon Systech

## Device

SSD1306, 128 x 64 dot-matrix OLED/PLED segment/common driver with controller.

## Project relevance

H2SSensor uses a common 128 x 64 I2C OLED module based on the SSD1306 controller.

## Key data

- Native controller matrix: 128 x 64
- I2C and SPI interfaces supported by the controller
- Integrated display RAM
- Internal charge-pump support
- Common I2C module addresses: `0x3C` and `0x3D`
- H2SSensor currently expects `0x3C`
- The OLED shares SDA GPIO21 and SCL GPIO22 with the ELT H2S sensor

## Voltage caution

The SSD1306 IC logic supply is a low-voltage device. Complete breakout modules vary: some include regulators and pull-up resistors and others do not. For this project, use a 3.3 V-compatible OLED module and ensure its I2C pull-ups are not tied to 5 V because the ELT H2S sensor and ESP32 use a 3.3 V bus.

## Documentation

Manufacturer product page:
https://www.solomon-systech.com/en/product/SSD1306

Widely used datasheet mirror:
https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
