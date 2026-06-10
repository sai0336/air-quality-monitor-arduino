# Air Quality Monitor with Real-Time Clock

A dual-Arduino environmental monitor that displays temperature, humidity, air quality, and current time across three screens on a TFT display.

Built this to have a small desktop device showing actual room conditions at a glance — not just serial monitor output, but a proper screen you can look at and immediately understand.

---

## How it works

Two Arduinos are used, each with a separate job.

**Arduino Nano** reads from all sensors and sends data to the Uno over serial communication.

**Arduino Uno** receives that data and handles everything on the display — it drives the TFT screen and manages the three screens.

Splitting the work keeps the display smooth and the sensor reading reliable.

---

## Screens

| Screen | What it shows |
|---|---|
| Clock | Current time and date from DS3231 RTC |
| Temperature | Live temperature (C) and humidity (%) from DHT11 |
| Air Quality | MQ135 reading with status indicator |

---

## Components

| Part | Purpose |
|---|---|
| Arduino Nano | Sensor node |
| Arduino Uno | Display controller |
| TFT Display (ST7735 / ILI9341) | Output screen |
| DS3231 RTC Module | Real-time clock with battery backup |
| DHT11 | Temperature and humidity sensor |
| MQ135 | Air quality sensor |

---

## Pin Connections

### Arduino Nano

| Component | Pin |
|---|---|
| DHT11 data | D2 |
| MQ135 analog out | A0 |
| DS3231 SDA | A4 |
| DS3231 SCL | A5 |
| TX to Uno | D1 |

### Arduino Uno

| Component | Pin |
|---|---|
| TFT CS | D10 |
| TFT DC | D9 |
| TFT RST | D8 |
| TFT MOSI | D11 |
| TFT SCK | D13 |
| RX from Nano | D0 |

---

## Libraries

```
Adafruit GFX Library
Adafruit ST7735 or ILI9341
DHT sensor library by Adafruit
RTClib by Adafruit
```

---

## How to run

1. Install all libraries from Arduino IDE Library Manager
2. Open `nano_sensor.ino` and upload to Arduino Nano
3. Open `uno_display.ino` and upload to Arduino Uno
4. Connect components as per pin diagram above
5. Power on — clock screen loads by default

---

## File structure

```
air-quality-monitor-arduino/
|
|-- nano_sensor/
|   |-- nano_sensor.ino
|
|-- uno_display/
|   |-- uno_display.ino
|
|-- README.md
```

---

Built by [Sai Chavan](https://github.com/sai0336)
