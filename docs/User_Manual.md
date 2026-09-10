# H2SSensor User Manual

**Firmware:** H2SSensor 0.1.x  
**Controller:** ESP32_MOS_X4  
**Sensor:** ELT Sensor H2S-SM30-3V (I2C)  
**Display:** 128×64 SSD1306 I2C OLED

> **Important safety limitation** — This firmware is an engineering prototype and is not a certified personnel-protection gas detector. Hydrogen sulphide (H2S) can be immediately dangerous to life and health. Do not rely on this project as the sole warning device. Calibration and alarm tests must be performed using suitable procedures, certified test gas and appropriate safety equipment.

## 1. Purpose

H2SSensor reads an ELT H2S-SM30-3V sensor over I2C, applies an optional software calibration, smooths the result with a rolling average, compares the filtered value to configurable LOW and HIGH alarm thresholds, drives two MOSFET lamp outputs, displays the local reading, serves a configuration/status website and optionally publishes selected tags by MQTT.

The controller is intended to remain useful without Wi-Fi. Its setup access point remains active while the instrument runs, and infrastructure Wi-Fi/MQTT are optional.

## 2. Hardware connections

### ESP32 and H2S-SM30-3V

Use the H2S-SM30-3V **3.3 V version** and maintain a common ground.

| ELT J3 pin | ELT function | ESP32_MOS_X4 connection |
|---:|---|---|
| 3 | GND | GND |
| 4 | VDD +3.3 V | regulated 3.3 V |
| 8 | I2C SCL | GPIO22 |
| 9 | I2C SDA | GPIO21 |
| 10 | Reset, low active | GPIO18 |
| 11 | Manual Span | GPIO19 |
| 13 | Manual Zero | GPIO23 |

The ELT documentation identifies I2C slave address `0x72` for the H2S module and describes an `R` command followed by a seven-byte response.

### OLED

| OLED | ESP32_MOS_X4 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

The firmware default OLED address is `0x3C`.

### Front-panel buttons

Connect each normally-open button between its GPIO and GND. Internal ESP32 pull-ups are enabled.

| Button | GPIO |
|---|---:|
| CAL | 32 |
| ZERO | 33 |
| RESET / ENTER | 25 |

### Alarm lamps

The ESP32_MOS_X4 MOSFET outputs switch the load side of the lamps. Confirm your exact board's output polarity and voltage arrangement before connecting a load.

| Alarm | MOSFET channel | GPIO |
|---|---|---:|
| LOW | OUT1 | 16 |
| HIGH | OUT2 | 17 |

Do not power a lamp from the ESP32 3.3 V regulator unless its current requirement is explicitly within the regulator/board rating. Normally the lamps use the MOSFET output supply rail appropriate to the ESP32_MOS_X4 module and load.

## 3. First power-up

1. Inspect all 3.3 V and ground wiring before applying power.
2. Power the instrument in clean air in a safe area.
3. The OLED should show `H2S ONLINE` once valid I2C readings are received.
4. The unit creates a Wi-Fi AP named `H2SSensor-xxxxxx`.
5. Connect a phone/laptop to that AP and browse to `192.168.4.1`.
6. Verify the status page reports sensible raw and filtered readings.
7. Confirm the ELT sensor and OLED both function on the shared I2C bus.
8. Perform a controlled bump/calibration test before trusting any alarm indication.

If the sensor does not produce valid readings, the firmware marks it OFFLINE after approximately five seconds without a successful sample and turns the software LOW/HIGH alarm outputs off. A production safety design should normally add a dedicated fault lamp/relay or fail-safe output; this prototype does not substitute a sensor-fault condition for a gas alarm.

## 4. Display

Normal display fields include:

- sensor ONLINE/OFFLINE condition
- filtered H2S concentration in ppm
- LOW and HIGH setpoints
- NORMAL, LOW ALARM, HIGH ALARM, or calibration activity
- Wi-Fi indication when connected

The displayed gas concentration is the **filtered, software-calibrated** value used for alarm decisions.

## 5. Alarm operation and rolling filter

The sensor is sampled once per second. The default rolling average contains 10 samples, so short single-sample disturbances are heavily reduced before they reach the alarm decision.

Alarm logic includes hysteresis. For example, with LOW = 5.0 ppm and hysteresis = 0.5 ppm:

- LOW alarm turns on when the filtered value reaches or exceeds 5.0 ppm.
- Once active, LOW alarm remains on until the filtered value falls to 4.5 ppm or lower.

HIGH works the same way using the HIGH setpoint.

The web **Alarms** page lets you set:

- LOW threshold
- HIGH threshold
- alarm clear hysteresis
- rolling-average length, 1–30 samples

Increasing the filter length reduces flicker/noise sensitivity but also increases response delay. Select the filter in accordance with the required alarm response time and validate it using controlled gas tests.

## 6. Local setpoint adjustment

To prevent accidental setpoint changes, local adjustment requires a two-button hold.

1. Hold **CAL + ZERO** together for at least 3 seconds.
2. The display enters `LOW SET`.
3. Press CAL to increase LOW by 0.5 ppm.
4. Press ZERO to decrease LOW by 0.5 ppm.
5. Press RESET/ENTER to accept LOW and move to `HIGH SET`.
6. Use CAL/ZERO to adjust HIGH.
7. Press RESET/ENTER to save and exit.

The firmware prevents HIGH from being saved less than or equal to LOW. Saved values are stored in ESP32 NVS flash and survive restart.

## 7. CAL, ZERO and RESET normal functions

When not in setpoint mode:

- a CAL button action starts the ELT manual span line for approximately 60 seconds;
- a ZERO button action starts the ELT manual zero line for approximately 60 seconds;
- RESET/ENTER pulses the ELT reset line low for approximately 300 ms.

ELT documentation revisions have shown different stated span concentrations (for example, 50 ppm in one revision and 100 ppm in a newer English revision). **Use the calibration concentration and procedure specified for the exact sensor supplied to you.** Do not assume the firmware's web-page example value is the required ELT hardware span concentration.

## 8. Software calibration

The firmware also includes an independent two-point software correction. It is stored in ESP32 flash and does not modify the sensor's internal factory calibration.

The calculation is conceptually:

`corrected = (raw - captured_raw_zero) × gain`

### Software zero

1. Establish the correct zero-gas condition.
2. Wait for the raw reading to stabilize.
3. Open **Calibration**.
4. Select **Capture software ZERO now**.

### Software span

1. Establish the known span gas concentration and allow the sensor to stabilize.
2. Enter the actual reference concentration on the Calibration page.
3. Select **Apply software SPAN**.

The gain is recalculated from the current raw value and the previously captured raw zero.

Use **Reset software calibration 1:1** to return to raw-zero = 0 and gain = 1.

For traceable instrumentation, record calibration gas lot/expiry, regulator/flow, ambient conditions, pre/post readings and calibration date separately; this initial firmware does not yet implement a calibration-history log.

## 9. Wi-Fi operation

The instrument uses concurrent ESP32 AP + station mode.

### Standalone mode

No external Wi-Fi is required. Connect directly to the `H2SSensor-xxxxxx` AP and use `192.168.4.1`.

### Join a network

1. Open **Wi-Fi**.
2. Select a scanned SSID.
3. Enter its password.
4. Select **Save & connect**.

The credential is stored in ESP32 NVS. If the infrastructure network is unavailable, the H2SSensor AP still remains accessible.

Select **Use standalone / forget Wi-Fi** to clear the stored SSID and stop station reconnect attempts.

## 10. Web pages

### Status `/`

Shows filtered ppm, instantaneous calibrated ppm, raw ppm, alarms, thresholds, I2C read statistics, station/AP networking and MQTT connection state.

### Wi-Fi `/wifi`

Scans networks and stores/clears infrastructure credentials.

### Alarms `/alarms`

Configures LOW/HIGH, hysteresis and rolling-average length.

### Calibration `/cal`

Provides software zero/span and controlled ELT hardware calibration/reset actions.

### MQTT `/mqtt`

Configures enable state, broker hostname/IP, port, username/password, base topic and minimum publish interval.

### Tags `/tags`

Allows each MQTT value to be independently enabled or disabled and its topic suffix edited.

### Instructions `/instructions`

Provides a quick local operating reference.

### API `/api/status`

Returns JSON containing firmware version, raw/filtered ppm, alarm states, setpoints, connectivity, read counters and the latest seven raw I2C response bytes.

## 11. MQTT configuration

MQTT is disabled by default. Configure:

- Enable MQTT
- Broker host or IP
- TCP port (default 1883)
- Optional username/password
- Base topic (default `sensors/h2s1`)
- Minimum publishing time in seconds

The minimum interval is enforced across the group of tag publishes.

The **Tags** page controls these values independently:

| Default suffix | Value |
|---|---|
| `h2s_ppm` | filtered/calibrated H2S ppm |
| `h2s_raw_ppm` | raw ELT value |
| `alarm_low` | `1` or `0` |
| `alarm_high` | `1` or `0` |
| `sensor_ok` | `1` or `0` |
| `low_setpoint` | LOW threshold |
| `high_setpoint` | HIGH threshold |

Messages are published retained so a newly connected supervisory client can see the latest state.

## 12. Flashing the ESP32_MOS_X4

The ESP32_MOS_X4 variants commonly found online use a USB-C connector for power but do not necessarily include USB-to-UART conversion. Use a suitable **3.3 V UART logic-level** programming adapter.

Typical programming connections:

- adapter TX → board RX
- adapter RX → board TX
- adapter GND → board GND
- power the board separately

To enter the ESP32 bootloader, hold the board's IO0 button low during reset/power-up or while PlatformIO begins connecting, then release it.

From the repository root:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

## 13. Commissioning test

A minimum commissioning test should include:

1. Sensor zero/clean-air reading verification.
2. Known test-gas exposure.
3. Raw reading comparison with expected sensor response.
4. Filtered value response-time measurement.
5. LOW lamp trip and clear verification.
6. HIGH lamp trip and clear verification.
7. Power-cycle and confirm thresholds/calibration persist.
8. Disconnect the I2C sensor and verify OFFLINE indication/read failures.
9. Test direct AP web access without external Wi-Fi.
10. Test infrastructure reconnection after AP/network loss.
11. If MQTT is enabled, verify every enabled tag and confirm disabled tags are not published.

## 14. Troubleshooting

### Sensor stays OFFLINE

Check 3.3 V supply, common ground, SDA/SCL orientation, I2C address `0x72` and the physical sensor revision. Disconnect other I2C devices temporarily and test the H2S module alone if necessary.

### OLED blank

Confirm 3.3 V, address `0x3C`, and SDA/SCL. Some OLEDs use `0x3D`; change the firmware constant if required.

### Alarm lamps reversed or always on

Verify which terminal is OUT+ and OUT− on your ESP32_MOS_X4 revision, the external load supply, and whether your board's MOSFET channels correspond to GPIO16 and GPIO17. Do not infer lamp polarity from firmware alone.

### Reading appears byte-swapped or unrealistic

Use `/api/status` and inspect the seven-byte `packet` array. The firmware defaults to the documented one-byte configuration + two-byte H2S layout and prefers big-endian decoding, with a plausibility fallback. If your sensor revision encodes the two H2S bytes differently, capture several raw packets at known concentrations before changing the driver.

### MQTT does not connect

Verify station Wi-Fi first, then broker address/port/credentials. MQTT is not attempted while the ESP32 is disconnected from infrastructure Wi-Fi.

## 15. Maintenance notes

- Periodically bump-test the installed sensor according to the applicable safety program.
- Replace electrochemical sensors according to manufacturer life/diagnostic recommendations.
- Keep calibration openings and diffusion paths unobstructed.
- Review alarm thresholds against site and regulatory requirements; the firmware defaults are placeholders only.
- Back up site-specific MQTT/topic/setpoint configuration separately if many units will be deployed.
