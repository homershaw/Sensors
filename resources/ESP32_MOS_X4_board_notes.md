# ESP32_MOS_X4 Board Notes

## Status

The exact manufacturer and revision-specific schematic for the ESP32_MOS_X4 controller board used by H2SSensor has not yet been verified from an official source.

## Current community-derived mapping

Community reverse-engineering reports the four MOSFET output channels as:

| Channel | ESP32 GPIO |
|---|---:|
| OUT1 | 16 |
| OUT2 | 17 |
| OUT3 | 26 |
| OUT4 | 27 |

H2SSensor currently uses:

- OUT1 / GPIO16: LOW alarm lamp
- OUT2 / GPIO17: HIGH alarm lamp

## Validation requirement

Before field deployment, verify with the actual board:

1. MOSFET outputs are low-side switches.
2. OUT1 is controlled by GPIO16.
3. OUT2 is controlled by GPIO17.
4. External supply voltage and load current are within the MOSFET/output-stage ratings.
5. Board and lamp supply grounds are common where required by the output topology.
6. No board-level circuitry conflicts with GPIO18, GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO32 or GPIO33.

## Community reference

Arduino forum discussion containing ESP32_MOS_X4 reverse-engineering information:
https://forum.arduino.cc/t/esp32-mos-x4-qualcuno-lo-conosce/1420533

Treat community pin mappings as provisional until physically confirmed.
