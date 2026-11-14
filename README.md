# Air Quality Display with Real-Time Clock

A two-Arduino environmental display using an **Arduino Nano** as the sensor/RTC node and an **Arduino Uno** driving a parallel TFT shield. The project shows a clock, temperature/humidity, and an experimental MQ135-based gas-sensor visualization.

> **Important measurement limitation:** The MQ135 produces **one analog reading** in this sketch. The Uno firmware derives six example values labelled CO2, CO, NH3, NO2, BENZ and SMOK by scaling that **same raw ADC value**. Those numbers are **not independent gas measurements or calibrated ppm values**. The displayed `SAFE`, `HIGH`, `GOOD`, etc. labels are illustrative thresholds, **not validated air-safety assessments**. Do not use this display to make health or safety decisions.

## Architecture

- **Nano:** Reads DS3231 RTC via I2C, DHT11 temperature and humidity, and MQ135 analog output. It sends a comma-separated serial packet to the Uno once per second at 9600 baud. It also reads an active-low button/contact input and controls a buzzer.
- **Uno:** Parses the serial packets and renders three screens on a **MCUFRIEND-compatible parallel TFT shield**.

## Screens

| Screen | Source |
| --- | --- |
| Clock | DS3231 time/date from Nano |
| Air visualization | MQ135 raw analog input; Uno uses illustrative scaling, not measured gas ppm |
| Temperature & humidity | DHT11 readings |

## Hardware and connections

### Arduino Nano — sensor node

| Device | Arduino Nano |
| --- | --- |
| DHT11 data | D2 |
| MQ135 analog output | A0 |
| DS3231 SDA | A4 |
| DS3231 SCL | A5 |
| Buzzer drive | D3 |
| Button/contact input (active LOW) | D4 |
| TX to Uno RX | D1 (TX) → Uno D0 (RX) |
| Ground | Shared GND with Uno |

Power sensor modules according to their individual specifications. Check the MQ135 module voltage, warm-up and calibration requirements before interpreting its reading.

### Arduino Uno — display node

- The sketch uses **`MCUFRIEND_kbv` and `Adafruit_GFX`**, intended for a compatible **parallel TFT shield** that plugs directly onto the Uno.
- It is **not** written for an SPI ST7735 display. The older README's SPI CS/DC/MOSI/SCK wiring table was incorrect for this sketch.
- Uno D0 (RX) receives Nano D1 (TX) at 9600 baud; connect grounds together.
- Disconnect or isolate the external serial link if it interferes with uploading sketches to the Uno. Avoid simultaneously driving a serial line from multiple devices.

## Arduino libraries

- **Nano sketch:** `DHT sensor library` (plus any required Adafruit dependencies); `Wire` comes with Arduino.
- **Uno sketch:** `Adafruit GFX Library` and `MCUFRIEND_kbv`.

The DS3231 is read directly over `Wire`, so **RTClib is not required** by these sketches.

## Upload procedure

1. Open `nano_sensor/nano_sensor.ino` in Arduino IDE; select **Arduino Nano** and the matching processor/port.
2. Install the DHT library and upload the Nano sketch.
3. Open `uno_display/uno_display.ino`; select **Arduino Uno**.
4. Install `Adafruit GFX` and `MCUFRIEND_kbv` and upload the Uno sketch.
5. Fit the compatible parallel TFT shield and connect Nano TX to Uno RX with shared ground.
6. Power the boards correctly and verify the clock, temperature/humidity, serial updates and button actions.

To initialize the RTC if needed, the Nano source has a commented `setRTC(...)` example. Set the correct time once and comment it again after uploading.

## Repository layout

```text
air-quality-monitor-arduino/
├── nano_sensor/
│   └── nano_sensor.ino
├── uno_display/
│   └── uno_display.ino
└── README.md
```

## Verification status

This repository cleanup **does not change** either sketch's program logic or confirm that it compiles/runs on your exact boards and TFT controller. The air-screen numbers remain illustrative until the measurement design is corrected and independently calibrated.

Built by [Sai Chavan](https://github.com/sai0336).
