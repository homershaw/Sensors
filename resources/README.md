# H2SSensor Reference Resources

This directory collects source material used for the H2SSensor project.

## Primary hardware references

### ELT Sensor H2S-SM30-3V

- Manufacturer: ELT Sensor Corp.
- Part: H2S-SM30-3V electrochemical hydrogen sulphide sensor module
- Current English datasheet (manufacturer):
  https://eltsensor.co.kr/user/attachment/202401/1704327462867677.pdf
- Older Korean datasheet, Ver. 1.62 (manufacturer):
  https://eltsensor.co.kr/user/attachment/202310/1696578449501290.pdf
- Local engineering notes: [`ELT_H2S-SM30-3V_reference.md`](ELT_H2S-SM30-3V_reference.md)

Important: the two available ELT revisions document different span-calibration concentrations. Always follow the datasheet supplied with the exact sensor revision installed in the instrument.

### ESP32-WROOM-32

- Manufacturer: Espressif Systems
- Module family used on ESP32_MOS_X4 boards
- Official datasheet:
  https://espressif.com/documentation/esp32-wroom-32_datasheet_en.pdf
- Official technical-document page:
  https://www.espressif.com/en/support/documents/technical-documents/esp32-wroom-32-datasheet
- Local engineering notes: [`ESP32-WROOM-32_reference.md`](ESP32-WROOM-32_reference.md)

### SSD1306 OLED controller

- Manufacturer: Solomon Systech
- Device: SSD1306 128 x 64 OLED/PLED controller
- Manufacturer product page:
  https://www.solomon-systech.com/en/product/SSD1306
- Datasheet mirror used by Adafruit and many open-source projects:
  https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
- Local engineering notes: [`SSD1306_reference.md`](SSD1306_reference.md)

### ESP32_MOS_X4 controller board

No verified manufacturer schematic/datasheet has been located for the exact ESP32_MOS_X4 board revision used by this project. Current MOSFET pin assignments are based on community reverse-engineering and must be validated on the physical board.

See [`ESP32_MOS_X4_board_notes.md`](ESP32_MOS_X4_board_notes.md).

## Generic components

The project also uses three normally-open momentary pushbuttons and two external alarm lamps. No manufacturer or part number has yet been specified for those items, so no component-specific datasheet is included. Once exact lamp/button part numbers are chosen, their datasheets should be added here.

## Suggested PDF filenames

When binary PDFs are added to this directory, use:

- `H2S-SM30-3V_ELT_Datasheet_English_v1.0.pdf`
- `H2S-SM30-3V_ELT_Datasheet_Korean_v1.62.pdf`
- `ESP32-WROOM-32_Datasheet_v3.7.pdf`
- `SSD1306_Datasheet.pdf`

## Source policy

Prefer manufacturer documentation. Third-party mirrors are retained only where the manufacturer does not provide a directly downloadable public document. Community information must be explicitly labelled as unverified until checked against the physical hardware.
