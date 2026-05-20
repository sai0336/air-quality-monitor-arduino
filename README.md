# 👀 Mimo Eyes & Air Quality Monitor — Dual Arduino Animated Display System

A dual-microcontroller project using **Arduino Nano** as a sensor node and **Arduino Uno** as an animated **TFT display controller** — showing real-time clock, temperature, humidity, and air quality with an animated character interface.

---

## 🎯 What It Does

- Displays an animated **"Mimo Eyes"** character on a TFT screen with multiple expressions
- Shows **real-time clock** (DS3231 RTC), **temperature & humidity** (DHT11), and **air quality** (MQ135) data
- Navigate between screens using **gesture-based menu**
- Plays a **boot animation** on startup with smooth screen transitions

---

## 🧰 Components Used

| Component | Purpose |
|---|---|
| Arduino Nano | Sensor node — reads all sensor data |
| Arduino Uno | Display controller — handles TFT & animations |
| TFT Display (ILI9341 / ST7735) | Main screen output |
| DS3231 RTC Module | Real-time clock (accurate timekeeping) |
| DHT11 Sensor | Temperature & Humidity reading |
| MQ135 Gas Sensor | Air quality / CO2 level monitoring |
| Jumper Wires | Connections between components |
| Breadboard | Prototyping |

---

## ⚙️ How It Works

```
[ Arduino Nano ]                [ Arduino Uno ]
   DHT11 Sensor       →            TFT Display
   MQ135 Sensor       →         Mimo Eyes Animation
   DS3231 RTC         →         Screen Navigation
   (Sensor Node)              (Display Controller)
```

- **Arduino Nano** reads sensor values and passes data to **Arduino Uno** via Serial communication
- **Arduino Uno** processes the data and renders it on the **TFT display**
- User navigates between 3 screens using gesture/button input

---

## 📺 Screens

### Screen 1 — 🕐 Clock
- Shows current time and date from DS3231 RTC
- Mimo Eyes displayed alongside

### Screen 2 — 🌡️ Temperature & Humidity
- Live temperature (°C) and humidity (%) from DHT11
- Mimo Eyes reacts based on temperature (happy/sad expression)

### Screen 3 — 💨 Air Quality
- Real-time air quality index from MQ135 sensor
- Shows Good / Moderate / Poor level
- Mimo Eyes reacts with worried expression on bad air quality

---

## 😄 Mimo Eyes Expressions

| Expression | Trigger |
|---|---|
| 😊 Happy | Normal temperature & good air |
| 😴 Sleepy | Idle / no interaction |
| 😠 Angry | High temperature detected |
| 😟 Worried | Poor air quality |
| 😉 Wink | Boot animation |
| 😐 Blink | Default idle blink loop |

---

## 🔌 Pin Connections

### Arduino Nano (Sensor Node)
| Sensor | Pin |
|---|---|
| DHT11 Data | D2 |
| MQ135 Analog Out | A0 |
| DS3231 SDA | A4 |
| DS3231 SCL | A5 |
| Serial TX to Uno | D1 (TX) |

### Arduino Uno (Display Controller)
| Module | Pin |
|---|---|
| TFT CS | D10 |
| TFT DC | D9 |
| TFT RST | D8 |
| TFT MOSI | D11 |
| TFT SCK | D13 |
| Serial RX from Nano | D0 (RX) |

---

## 📚 Libraries Required

Install these from Arduino IDE → Library Manager:

```
- Adafruit GFX Library
- Adafruit ST7735 / ILI9341 (based on your TFT)
- DHT sensor library (Adafruit)
- RTClib (Adafruit)
- MQ135 Library
```

---

## 🚀 How to Run

1. Clone or download this repository
2. Open Arduino IDE
3. Install all required libraries (listed above)
4. Open `nano_sensor_node.ino` → Upload to **Arduino Nano**
5. Open `uno_display_controller.ino` → Upload to **Arduino Uno**
6. Connect components as per pin diagram above
7. Power on — boot animation plays, then main screen loads!

---

## 📁 File Structure

```
mimo-eyes-arduino/
│
├── nano_sensor_node/
│   └── nano_sensor_node.ino       # Sensor reading code for Nano
│
├── uno_display_controller/
│   └── uno_display_controller.ino # TFT display & animation code for Uno
│
└── README.md
```

---

## 🌟 Features

- ✅ Dual microcontroller architecture
- ✅ Animated character with 6 expressions
- ✅ Real-time sensor data display
- ✅ Gesture-based screen navigation
- ✅ Boot animation on startup
- ✅ Smooth screen transitions

---

## 👨‍💻 Made By

**Sai Santosh Chavan**
B.Sc. IT Student | Mumbai

