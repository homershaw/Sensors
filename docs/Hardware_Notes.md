# H2SSensor Hardware / Protocol Notes

These notes capture the assumptions used by the initial firmware so bench validation is straightforward.

## ELT H2S-SM30-3V

Manufacturer datasheets located during initial development identify:

- supply: 3.3 V model
- electrochemical H2S measurement
- I2C slave address: `0x72`
- I2C operation: write ASCII `R` (`0x52`), then read seven bytes
- response layout shown as 1 byte configuration, 2 bytes H2S, and 4 reserved bytes
- board-to-board / side-hole access to I2C, reset, manual zero and manual span
- manual reset is active LOW
- manual zero and manual span procedures are approximately one minute

The initial firmware interprets response bytes 1 and 2 as an unsigned H2S value. It prefers big-endian order and contains a conservative byte-swap plausibility fallback. The `/api/status` endpoint exposes all seven bytes so this can be confirmed on real hardware before the decoder is considered validated.

There are ELT datasheet revisions that differ in the stated manual span gas concentration (50 ppm in an older revision versus 100 ppm in a newer English revision). The firmware therefore does not hard-code a claim that either concentration is correct for every module. Follow the label/datasheet supplied with the exact sensor.

Manufacturer reference used during design:

`https://eltsensor.co.kr/user/attachment/202401/1704327462867677.pdf`

## ESP32_MOS_X4

Public documentation for the board is limited. Community reverse-engineering reports these MOSFET mappings:

| Output | GPIO |
|---|---:|
| OUT1 | 16 |
| OUT2 | 17 |
| OUT3 | 26 |
| OUT4 | 27 |

The H2SSensor uses OUT1 for LOW alarm and OUT2 for HIGH alarm.

The board image/community pin labels show GPIO21, GPIO22, GPIO25, GPIO32, GPIO33, GPIO18, GPIO19 and GPIO23 available on headers, which are used for I2C, buttons and sensor control in the initial design.

Community references used during initial design:

- `https://forum.arduino.cc/t/esp32-mos-x4-qualcuno-lo-conosce/1420533`
- `https://www.reddit.com/r/esp32/comments/1p48fdq/i_need_some_confirmation_before_flashing_this/`

## Required bench validation before release

1. Confirm the exact ESP32_MOS_X4 PCB revision and trace GPIO16/GPIO17 to OUT1/OUT2.
2. Confirm all selected header GPIOs are exposed on the actual board.
3. I2C-scan the finished wiring and verify addresses `0x72` and `0x3C`.
4. Capture the seven H2S bytes at clean air/zero and at at least two known H2S concentrations.
5. Verify byte order and engineering-unit scaling.
6. Confirm manual SPAN/ZERO inputs idle safely HIGH and activate from ESP32 GPIO LOW.
7. Confirm sensor reset behavior.
8. Measure alarm lamp load current and board/MOSFET temperature.
9. Validate rolling-average length against required response time.
