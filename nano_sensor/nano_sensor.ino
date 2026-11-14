// ============================================================
// ARDUINO NANO — SENSOR NODE
// Pins:
//   DS3231  SDA→A4, SCL→A5
//   DHT11   → D2
//   MQ135   → A0
//   Buzzer  → D3
//   Touch   → D4
//
// Gesture logic:
//   TAP    < 200ms          → BTN:1   (cycle screen)
//   DTAP   2 taps < 350ms   → DTAP:1
//   LBTN   600–1500ms       → LBTN:1
//   SLBTN  > 1500ms release → SLBTN:1
// ============================================================

#include <Wire.h>
#include <DHT.h>

struct MyTime {
    uint8_t ss, mm, hh, dow, day, month;
    uint16_t year;
};

#define DHTPIN      2
#define DHTTYPE     DHT11
#define MQPIN       A0
#define BUZZERPIN   3
#define TOUCHPIN    4
#define DS3231_ADDR 0x68

#define TAP_MAX    200
#define LONG_MIN   600
#define SUPER_MIN 1500
#define DTAP_GAP   350

DHT dht(DHTPIN, DHTTYPE);

unsigned long prevSend   = 0;
unsigned long prevBuzzer = 0;
bool buzzerState = false;
unsigned long buzzerOn = 0, buzzerOff = 0;
int buzzerFreq = 0;

bool lastTouch   = false;
bool isPressed   = false;
unsigned long pressStart = 0;

bool flag_btn   = false;
bool flag_dtap  = false;
bool flag_lbtn  = false;
bool flag_slbtn = false;

bool waitingDtap      = false;
unsigned long lastTapTime = 0;

float cachedTemp = 0;
int   cachedRaw  = 0;

uint8_t bcd2dec(uint8_t v) { return (v / 16 * 10) + (v % 16); }
uint8_t dec2bcd(uint8_t v) { return (v / 10 * 16) + (v % 10); }

// ============================================================
// RTC
// ============================================================
MyTime readRTC() {
    MyTime t;
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(0x00);
    Wire.endTransmission();
    Wire.requestFrom(DS3231_ADDR, 7);
    t.ss    = bcd2dec(Wire.read() & 0x7F);
    t.mm    = bcd2dec(Wire.read());
    t.hh    = bcd2dec(Wire.read() & 0x3F);
    t.dow   = bcd2dec(Wire.read());
    t.day   = bcd2dec(Wire.read());
    t.month = bcd2dec(Wire.read() & 0x1F);
    t.year  = bcd2dec(Wire.read()) + 2000;
    return t;
}

void setRTC(uint8_t ss, uint8_t mm, uint8_t hh,
            uint8_t dow, uint8_t day, uint8_t month, uint8_t year) {
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(0x00);
    Wire.write(dec2bcd(ss));   Wire.write(dec2bcd(mm));
    Wire.write(dec2bcd(hh));   Wire.write(dec2bcd(dow));
    Wire.write(dec2bcd(day));  Wire.write(dec2bcd(month));
    Wire.write(dec2bcd(year));
    Wire.endTransmission();
}

// ============================================================
// SENSOR BUZZER — non-blocking
// ============================================================
void updateSensorBuzzer(float temp, int raw) {
    bool highTemp = (temp > 35.0);
    bool badAir   = (raw > 600);
    unsigned long now = millis();

    if (!highTemp && !badAir) {
        noTone(BUZZERPIN);
        buzzerState = false;
        return;
    }

    if (highTemp && badAir)  { buzzerOn=150; buzzerOff=100; buzzerFreq=buzzerState?1000:500; }
    else if (highTemp)       { buzzerOn=100; buzzerOff=100; buzzerFreq=1000; }
    else                     { buzzerOn=500; buzzerOff=500; buzzerFreq=500; }

    unsigned long elapsed = now - prevBuzzer;
    if (buzzerState) {
        if (elapsed >= buzzerOn)  { noTone(BUZZERPIN); buzzerState=false; prevBuzzer=now; }
    } else {
        if (elapsed >= buzzerOff) { tone(BUZZERPIN,buzzerFreq); buzzerState=true; prevBuzzer=now; }
    }
}

// ============================================================
// GESTURE SOUND
// ============================================================
void playTap() {
    tone(BUZZERPIN, 900, 50);
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(9600);
    Wire.begin();
    dht.begin();
    pinMode(TOUCHPIN, INPUT_PULLUP);
    pinMode(BUZZERPIN, OUTPUT);

    delay(2000);

    // Uncomment ONCE to set time, then comment back and re-upload:
    // setRTC(0, 0, 12, 2, 8, 4, 26);
}

// ============================================================
// LOOP
// ============================================================
void loop() {
    unsigned long now = millis();
    bool currentTouch = (digitalRead(TOUCHPIN) == LOW);

    // Button pressed
    if (currentTouch && !lastTouch) {
        pressStart = now;
        isPressed  = true;
    }

    // Button released — classify gesture
    if (!currentTouch && lastTouch && isPressed) {
        unsigned long dur = now - pressStart;
        isPressed = false;

        if (dur >= SUPER_MIN) {
            flag_slbtn = true;

        } else if (dur >= LONG_MIN) {
            flag_lbtn = true;

        } else if (dur < TAP_MAX) {
            if (waitingDtap && (now - lastTapTime) < DTAP_GAP) {
                flag_dtap   = true;
                waitingDtap = false;
            } else {
                waitingDtap = true;
                lastTapTime = now;
            }
        }
    }

    lastTouch = currentTouch;

    // Resolve single tap after double-tap wait window expires
    if (waitingDtap && (now - lastTapTime) >= DTAP_GAP) {
        waitingDtap = false;
        flag_btn    = true;
        playTap();
    }

    // Send packet every 1 second
    if (now - prevSend >= 1000) {
        prevSend = now;

        MyTime t   = readRTC();
        float temp = dht.readTemperature();
        float humi = dht.readHumidity();
        int   raw  = analogRead(MQPIN);

        if (isnan(temp)) temp = 0.0;
        if (isnan(humi)) humi = 0.0;
        cachedTemp = temp;
        cachedRaw  = raw;

        Serial.print(F("T:"));     Serial.print(temp, 1);
        Serial.print(F(",H:"));    Serial.print(humi, 1);
        Serial.print(F(",A:"));    Serial.print(raw);
        Serial.print(F(",HH:"));   Serial.print(t.hh);
        Serial.print(F(",MM:"));   Serial.print(t.mm);
        Serial.print(F(",SS:"));   Serial.print(t.ss);
        Serial.print(F(",D:"));    Serial.print(t.day);
        Serial.print(F(",Mo:"));   Serial.print(t.month);
        Serial.print(F(",Y:"));    Serial.print(t.year);
        Serial.print(F(",W:"));    Serial.print(t.dow);
        Serial.print(F(",BTN:"));  Serial.print(flag_btn   ? 1 : 0);
        Serial.print(F(",DTAP:")); Serial.print(flag_dtap  ? 1 : 0);
        Serial.print(F(",LBTN:")); Serial.print(flag_lbtn  ? 1 : 0);
        Serial.print(F(",SLBTN:"));Serial.print(flag_slbtn ? 1 : 0);
        Serial.println();

        flag_btn = flag_dtap = flag_lbtn = flag_slbtn = false;
    }

    updateSensorBuzzer(cachedTemp, cachedRaw);
}
