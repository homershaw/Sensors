# ELT H2S-SM30-3V Reference

## Manufacturer

ELT Sensor Corp.

## Function

Electrochemical hydrogen sulphide sensor module with UART, I2C, PWM and alarm output options.

## Key specifications from manufacturer documentation

- Measurement range: 0 to 100 ppm H2S
- Maximum overload: 300 ppm
- Accuracy: approximately ±3% full scale
- Resolution: 0.1 ppm
- Response: T90 < 30 s, T60 < 9 s
- Sampling interval: 1 s
- Warm-up: < 60 s for rated accuracy
- Supply: 3.3 VDC, approximately 3.2 to 3.5 V range
- Current consumption: < 6.6 mA
- Operating temperature: approximately -20 to +50 °C
- Operating humidity: 15 to 90% RH non-condensing for standard model
- Expected electrochemical cell life: approximately 2 years under documented conditions

## I2C interface used by H2SSensor

- 7-bit I2C slave address: `0x72`
- Host writes ASCII `R` (`0x52`)
- Sensor returns 7 bytes
- Documented packet organization: configuration/status byte, two H2S data bytes, then four reserved bytes

The firmware retains the complete seven-byte packet for commissioning diagnostics.

## Calibration controls

The module provides external manual ZERO, SPAN and RESET inputs. These inputs are treated as active-low in the current prototype design.

Important: available ELT datasheet revisions do not agree on the span-gas concentration. One revision documents 50 ppm while another English revision documents 100 ppm. Do not perform span calibration until the exact installed sensor revision and its calibration procedure are confirmed.

## Official documents

Current English datasheet:
https://eltsensor.co.kr/user/attachment/202401/1704327462867677.pdf

Older Korean datasheet Ver. 1.62:
https://eltsensor.co.kr/user/attachment/202310/1696578449501290.pdf

## Project use

The sensor shares the ESP32 I2C bus with the SSD1306 OLED. Current project pin assignment is SDA GPIO21 and SCL GPIO22.
