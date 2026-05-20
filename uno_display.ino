// ============================================================
// ARDUINO UNO — DISPLAY NODE v3
// TFT Shield: MCUFRIEND (ILI9341), pins 2–9, A0–A4
// Nano TX → Uno RX (pin 0)
//
// Two Desktops:
//   Desktop 0 — Sensor (Clock / Air / Temp)
//               TAP   = cycle screens
//               LBTN  = go to Eye Desktop
//
//   Desktop 1 — Mimo Eyes
//               TAP   = surprised
//               DTAP  = happy
//               LBTN  = angry (600ms)
//               SLBTN = happy flash then back to Desktop 0 (>1500ms)
//
// Boot sequence plays on power-on before entering Desktop 0.
// ============================================================

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

// ============================================================
// EYE GEOMETRY
// ============================================================
#define EYE_BG   0x0000   // black background
#define LE_X     42       // Left  eye rect X
#define RE_X     183      // Right eye rect X
#define EY       78       // Both eyes rect Y
#define EW       95       // Eye width
#define EH       68       // Eye height
#define ER       16       // Corner radius
#define LC_X     89       // Left  eye centre X
#define RC_X     230      // Right eye centre X
#define EC_Y     112      // Eye centre Y
#define PUP_R    19       // Pupil radius
#define SHINE_R  6        // Pupil shine dot radius

// Eye states
#define ES_IDLE      0
#define ES_BLINK     1
#define ES_HAPPY     2
#define ES_ANGRY     3
#define ES_SURPRISED 4
#define ES_SLEEPY    5
#define ES_WINK      6

// ============================================================
// GLOBAL STATE
// ============================================================
MCUFRIEND_kbv tft;

// ---- Desktop ----
int currentDesktop = 0;   // 0=Sensor, 1=Eyes
int currentMode    = 0;   // Sensor: 0=Clock 1=Air 2=Temp

// ---- Sensor cache ----
uint8_t last_ss=255, last_mm=255, last_hh=255;
float last_temp=-999, last_humi=-999;
int   last_raw=-1;

// ---- Cat animation ----
unsigned long catMillis = 0;
int catFrame = 0;

// ---- Serial ----
String serialBuffer = "";

// ---- Parsed packet values ----
float  val_temp=0, val_humi=0;
int    val_raw=0;
struct MyTime { uint8_t ss,mm,hh,dow,day,month; uint16_t year; } val_time;
bool val_btn=false, val_dtap=false, val_lbtn=false, val_slbtn=false;

// ---- Eye state machine ----
int  eyeState      = ES_IDLE;
unsigned long eyeStateStart = 0;
unsigned long eyeDuration   = 0;   // how long to hold the state

// ---- Idle blink ----
unsigned long nextBlink  = 0;
int  blinkPhase = 0;
unsigned long blinkTimer = 0;

// ---- Pupil drift ----
int  pupX=0, pupY=0;           // current
int  pupTX=0, pupTY=0;         // target
unsigned long nextPupilMove = 0;
unsigned long lastPupilStep = 0;

// ---- Sleepy idle timer ----
unsigned long lastInteraction = 0;
#define SLEEPY_MS 30000UL

// ---- Hearts animation ----
int heartFrame   = 0;
unsigned long heartTimer = 0;

// ============================================================
// PACKET PARSER
// ============================================================
void parsePacket(String &pkt) {
    auto extract = [&](const char* key) -> String {
        String k = String(key) + ":";
        int idx = pkt.indexOf(k);
        if (idx == -1) return F("0");
        idx += k.length();
        int end = pkt.indexOf(',', idx);
        if (end == -1) end = pkt.length();
        return pkt.substring(idx, end);
    };
    val_temp       = extract("T").toFloat();
    val_humi       = extract("H").toFloat();
    val_raw        = extract("A").toInt();
    val_time.hh    = extract("HH").toInt();
    val_time.mm    = extract("MM").toInt();
    val_time.ss    = extract("SS").toInt();
    val_time.day   = extract("D").toInt();
    val_time.month = extract("Mo").toInt();
    val_time.year  = extract("Y").toInt();
    val_time.dow   = extract("W").toInt();
    val_btn        = (extract("BTN").toInt()  == 1);
    val_dtap       = (extract("DTAP").toInt() == 1);
    val_lbtn       = (extract("LBTN").toInt() == 1);
    val_slbtn      = (extract("SLBTN").toInt()== 1);
}

void resetCache() {
    last_ss=255; last_mm=255; last_hh=255;
    last_temp=-999; last_humi=-999; last_raw=-1;
}

uint8_t to12hr(uint8_t hh24, bool &isPM) {
    isPM = (hh24 >= 12);
    uint8_t h = hh24 % 12;
    return (h == 0) ? 12 : h;
}

// ============================================================
// ============================================================
// MIMO EYE DRAWING
// ============================================================
// ============================================================

// Draw base white eye shape for both eyes
void eyeBase() {
    tft.fillRoundRect(LE_X, EY, EW, EH, ER, 0xFFFF);
    tft.fillRoundRect(RE_X, EY, EW, EH, ER, 0xFFFF);
}

// Draw pupils with shine at given offset
void drawPupils(int ox, int oy) {
    // Left pupil
    tft.fillCircle(LC_X+ox, EC_Y+oy, PUP_R, 0x0000);
    tft.fillCircle(LC_X+ox-5, EC_Y+oy-6, SHINE_R, 0xFFFF);
    // Right pupil
    tft.fillCircle(RC_X+ox, EC_Y+oy, PUP_R, 0x0000);
    tft.fillCircle(RC_X+ox-5, EC_Y+oy-6, SHINE_R, 0xFFFF);
}

// Draw one pupil
void drawOnePupil(int cx, int ox, int oy) {
    tft.fillCircle(cx+ox, EC_Y+oy, PUP_R, 0x0000);
    tft.fillCircle(cx+ox-5, EC_Y+oy-6, SHINE_R, 0xFFFF);
}

void clearEyeArea() {
    tft.fillRect(0, 40, 320, 180, EYE_BG);
}

// ---- IDLE eyes ----
void drawIdle(int ox, int oy) {
    clearEyeArea();
    eyeBase();
    drawPupils(ox, oy);
}

// ---- BLINK (single frame — pass coverage 0..EH) ----
void drawBlink(int cover) {
    eyeBase();
    // Cover from top and bottom symmetrically
    int topCover = cover / 2;
    int botCover = cover / 2;
    tft.fillRect(LE_X, EY,            EW, topCover, EYE_BG);
    tft.fillRect(LE_X, EY+EH-botCover,EW, botCover, EYE_BG);
    tft.fillRect(RE_X, EY,            EW, topCover, EYE_BG);
    tft.fillRect(RE_X, EY+EH-botCover,EW, botCover, EYE_BG);
}

// ---- HAPPY eyes (^_^) ----
void drawHappy() {
    clearEyeArea();
    eyeBase();
    // Clip top 55% to get bottom-arc ^ shape
    tft.fillRect(LE_X, EY, EW, EH*55/100, EYE_BG);
    tft.fillRect(RE_X, EY, EW, EH*55/100, EYE_BG);
    // Small rounded pupils in lower arc
    tft.fillCircle(LC_X, EC_Y+10, 10, 0x0000);
    tft.fillCircle(RC_X, EC_Y+10, 10, 0x0000);
    // Rosy cheeks
    tft.fillCircle(LC_X-25, EC_Y+28, 12, 0xFC10);
    tft.fillCircle(RC_X+25, EC_Y+28, 12, 0xFC10);
    // Happy text
    tft.setTextSize(1);
    tft.setTextColor(0xFD20, EYE_BG);
    tft.setCursor(128, EC_Y+32);
    tft.print(F("^_^"));
}

// ---- ANGRY eyes ----
void drawAngry() {
    clearEyeArea();
    // Red tinted background hint
    tft.fillRect(0, 40, 320, 180, 0x1000);
    eyeBase();
    // Clip inner-top corners to create angry angle
    tft.fillTriangle(LC_X, EY, LE_X+EW, EY, LC_X, EY+28, EYE_BG);
    tft.fillTriangle(RC_X, EY, RE_X,    EY, RC_X, EY+28, EYE_BG);
    // Red eyebrows — angled inward-downward
    for (int i = 0; i < 6; i++) {
        tft.drawLine(LE_X+5,    EY-18-i, LE_X+EW-5, EY-6-i,  0xF800);
        tft.drawLine(RE_X+5,    EY-6-i,  RE_X+EW-5, EY-18-i, 0xF800);
    }
    // Red pupils
    tft.fillCircle(LC_X, EC_Y, PUP_R, 0xF800);
    tft.fillCircle(RC_X, EC_Y, PUP_R, 0xF800);
    tft.fillCircle(LC_X-5, EC_Y-6, SHINE_R-2, 0xFFFF);
    tft.fillCircle(RC_X-5, EC_Y-6, SHINE_R-2, 0xFFFF);
    // Grr text
    tft.setTextSize(2);
    tft.setTextColor(0xF800, 0x1000);
    tft.setCursor(130, EC_Y+35);
    tft.print(F(">:("));
}

// ---- SURPRISED eyes ----
void drawSurprised() {
    clearEyeArea();
    // Large circle eyes
    tft.fillCircle(LC_X, EC_Y, 38, 0xFFFF);
    tft.fillCircle(RC_X, EC_Y, 38, 0xFFFF);
    // Small pupils
    tft.fillCircle(LC_X, EC_Y, 12, 0x0000);
    tft.fillCircle(RC_X, EC_Y, 12, 0x0000);
    tft.fillCircle(LC_X-4, EC_Y-5, 4, 0xFFFF);
    tft.fillCircle(RC_X-4, EC_Y-5, 4, 0xFFFF);
    // Eyebrows raised
    for (int i = 0; i < 5; i++) {
        tft.drawFastHLine(LC_X-18, EY-22-i, 36, 0x8410);
        tft.drawFastHLine(RC_X-18, EY-22-i, 36, 0x8410);
    }
    // ! text
    tft.setTextSize(2);
    tft.setTextColor(0xFFE0, EYE_BG);
    tft.setCursor(148, EC_Y+42);
    tft.print(F("!"));
}

// ---- SLEEPY eyes ----
void drawSleepy() {
    clearEyeArea();
    eyeBase();
    // Heavy drooping eyelid — cover top 65%
    tft.fillRect(LE_X, EY, EW, EH*65/100, EYE_BG);
    tft.fillRect(RE_X, EY, EW, EH*65/100, EYE_BG);
    // Droopy pupils looking slightly down
    tft.fillCircle(LC_X, EC_Y+14, 10, 0x0000);
    tft.fillCircle(RC_X, EC_Y+14, 10, 0x0000);
    // ZZZ
    tft.setTextColor(0x07FF, EYE_BG);
    tft.setTextSize(1); tft.setCursor(LC_X+12, EY-14); tft.print(F("z"));
    tft.setTextSize(2); tft.setCursor(LC_X+22, EY-26); tft.print(F("z"));
    tft.setTextSize(1); tft.setCursor(RC_X+12, EY-14); tft.print(F("z"));
    tft.setTextSize(2); tft.setCursor(RC_X+22, EY-26); tft.print(F("z"));
}

// ---- WINK (left eye winks) ----
void drawWink() {
    clearEyeArea();
    // Right eye normal
    tft.fillRoundRect(RE_X, EY, EW, EH, ER, 0xFFFF);
    drawOnePupil(RC_X, 0, 0);
    // Left eye = thin horizontal line
    tft.fillRect(LE_X, EC_Y-3, EW, 7, 0xFFFF);
    // Wink text
    tft.setTextSize(2);
    tft.setTextColor(0xFFE0, EYE_BG);
    tft.setCursor(128, EC_Y+35);
    tft.print(F(";)"));
}

// ---- Heart shape helper ----
void drawHeart(int cx, int cy, int size, uint16_t col) {
    tft.fillCircle(cx - size/2, cy, size/2, col);
    tft.fillCircle(cx + size/2, cy, size/2, col);
    tft.fillTriangle(cx - size, cy, cx + size, cy, cx, cy + size + 2, col);
}

// ---- HAPPY with hearts (double tap) ----
void drawHappyHearts(int frame) {
    drawHappy();
    // Animate floating hearts
    int offset = (frame % 3) * 6;
    drawHeart(LC_X - 35, EY - 20 - offset, 8, 0xF81F);
    drawHeart(RC_X + 35, EY - 20 - offset, 8, 0xF81F);
    if (frame > 1) drawHeart(160, EY - 35 - offset, 6, 0xFC10);
}

// ---- Eye status bar ----
void drawEyeBar() {
    tft.fillRect(0, 0, 320, 20, 0x0819);
    tft.drawFastHLine(0, 20, 320, 0x2945);
    tft.setTextSize(1);
    tft.setTextColor(0x07FF, 0x0819);
    tft.setCursor(5, 6);
    tft.print(F("MIMO"));
    tft.setTextColor(0x4208, 0x0819);
    tft.setCursor(90, 6);
    tft.print(F("TAP=react  DTAP=happy  LONG=angry  VLONG=exit"));
}

// ---- Eye hint bar ----
void drawEyeHintBar() {
    tft.fillRect(0, 222, 320, 18, 0x0819);
    tft.drawFastHLine(0, 222, 320, 0x2945);
    tft.setTextSize(1);
    tft.setTextColor(0x4208, 0x0819);
    tft.setCursor(70, 228);
    tft.print(F("Hold 1.5s to return to sensor mode"));
}

// ---- Set a new eye state ----
void setEyeState(int newState, unsigned long duration) {
    eyeState      = newState;
    eyeStateStart = millis();
    eyeDuration   = duration;
    if (newState != ES_SLEEPY) lastInteraction = millis();
}

// ---- Enter eye desktop ----
void enterEyeDesktop() {
    tft.fillScreen(EYE_BG);
    drawEyeBar();
    drawEyeHintBar();
    pupX = pupY = pupTX = pupTY = 0;
    blinkPhase = 0;
    nextBlink = millis() + 3000;
    lastInteraction = millis();
    setEyeState(ES_IDLE, 0);
    drawIdle(0, 0);
}

// ============================================================
// BOOT ANIMATION
// ============================================================
void bootAnimation() {
    tft.fillScreen(0x0000);
    delay(300);

    // Eyes grow from dots
    for (int r = 3; r <= 38; r += 3) {
        tft.fillScreen(0x0000);
        tft.fillCircle(LC_X, EC_Y, r, 0xFFFF);
        tft.fillCircle(RC_X, EC_Y, r, 0xFFFF);
        delay(40);
    }

    // Full idle eyes
    drawIdle(0, 0);
    delay(400);

    // Look left
    drawIdle(-14, 0);
    delay(400);

    // Look right
    drawIdle(14, 0);
    delay(400);

    // Look center
    drawIdle(0, 0);
    delay(300);

    // Wink
    drawWink();
    delay(500);

    // Back to idle
    drawIdle(0, 0);
    delay(200);

    // Quick blinks x2
    for (int b = 0; b < 2; b++) {
        for (int c = 0; c <= EH; c += 12) { drawBlink(c); delay(25); }
        for (int c = EH; c >= 0; c -= 12) { drawBlink(c); delay(25); }
        delay(150);
    }

    // Happy moment
    drawHappy();
    delay(700);

    delay(200);
    // Transition to clock — fillScreen done in drawClockBG()
}

// ============================================================
// ============================================================
// SENSOR SCREENS (unchanged from original)
// ============================================================
// ============================================================

void drawModeBar(int mode) {
    tft.fillRect(0, 220, 320, 20, 0x0000);
    tft.drawFastHLine(0, 220, 320, 0x2945);
    tft.fillCircle(100, 229, 4, mode==0?0x07FF:0x2945);
    tft.fillCircle(160, 229, 4, mode==1?0xF800:0x2945);
    tft.fillCircle(220, 229, 4, mode==2?0x07E0:0x2945);
    tft.setTextSize(1);
    tft.setTextColor(mode==0?0x07FF:0x4208, 0x0000); tft.setCursor(108,225); tft.print(F("CLOCK"));
    tft.setTextColor(mode==1?0xF800:0x4208, 0x0000); tft.setCursor(167,225); tft.print(F("AIR"));
    tft.setTextColor(mode==2?0x07E0:0x4208, 0x0000); tft.setCursor(228,225); tft.print(F("TEMP"));
}

// ---- CAT ANIMATION ----
void drawCat(int frame) {
    int cx=60, cy=150;
    uint16_t catCol=0xFD20, darkCol=0xC9A0, bgCol=0x0000, eyeCol=0x07FF, noseCol=0xF81F;
    tft.fillRect(0,118,215,52,bgCol);
    tft.fillRoundRect(cx-18,cy-10,36,28,8,catCol);
    tft.fillCircle(cx,cy-22,16,catCol);
    tft.fillTriangle(cx-14,cy-34,cx-20,cy-48,cx-5,cy-35,catCol);
    tft.fillTriangle(cx-13,cy-35,cx-17,cy-44,cx-7,cy-36,darkCol);
    tft.fillTriangle(cx+14,cy-34,cx+20,cy-48,cx+5,cy-35,catCol);
    tft.fillTriangle(cx+13,cy-35,cx+17,cy-44,cx+7,cy-36,darkCol);
    if (frame==0||frame==1) {
        tft.fillCircle(cx-6,cy-24,4,eyeCol); tft.fillCircle(cx+6,cy-24,4,eyeCol);
        tft.fillCircle(cx-6,cy-24,2,0x0000); tft.fillCircle(cx+6,cy-24,2,0x0000);
        tft.fillCircle(cx-5,cy-25,1,0xFFFF); tft.fillCircle(cx+7,cy-25,1,0xFFFF);
    } else {
        tft.fillRect(cx-9,cy-25,6,3,bgCol); tft.fillRect(cx+3,cy-25,6,3,bgCol);
        tft.drawLine(cx-9,cy-22,cx-6,cy-26,0x0000); tft.drawLine(cx-6,cy-26,cx-3,cy-22,0x0000);
        tft.drawLine(cx+3,cy-22,cx+6,cy-26,0x0000); tft.drawLine(cx+6,cy-26,cx+9,cy-22,0x0000);
    }
    tft.fillTriangle(cx-2,cy-18,cx+2,cy-18,cx,cy-16,noseCol);
    tft.drawLine(cx,cy-16,cx-4,cy-13,0xFFFF); tft.drawLine(cx,cy-16,cx+4,cy-13,0xFFFF);
    tft.drawLine(cx-4,cy-17,cx-18,cy-19,0xFFFF); tft.drawLine(cx-4,cy-16,cx-18,cy-15,0xFFFF);
    tft.drawLine(cx+4,cy-17,cx+18,cy-19,0xFFFF); tft.drawLine(cx+4,cy-16,cx+18,cy-15,0xFFFF);
    tft.fillRoundRect(cx-15,cy+16,12,8,4,catCol); tft.fillRoundRect(cx+3,cy+16,12,8,4,catCol);
    tft.drawLine(cx-12,cy+23,cx-12,cy+24,darkCol); tft.drawLine(cx-9,cy+23,cx-9,cy+24,darkCol);
    tft.drawLine(cx+6,cy+23,cx+6,cy+24,darkCol);  tft.drawLine(cx+9,cy+23,cx+9,cy+24,darkCol);
    if (frame==0||frame==2) {
        tft.drawLine(cx+18,cy+10,cx+35,cy-5,catCol); tft.drawLine(cx+18,cy+11,cx+35,cy-4,catCol);
        tft.drawLine(cx+18,cy+12,cx+35,cy-3,catCol); tft.fillCircle(cx+35,cy-5,5,catCol);
    } else {
        tft.drawLine(cx+18,cy+10,cx+30,cy+5,catCol); tft.drawLine(cx+18,cy+11,cx+30,cy+6,catCol);
        tft.drawLine(cx+18,cy+12,cx+30,cy+7,catCol); tft.fillCircle(cx+30,cy+5,5,catCol);
    }
    if (frame==0) {
        tft.fillCircle(cx+55,cy-30,3,0xF800); tft.fillCircle(cx+59,cy-30,3,0xF800);
        tft.fillTriangle(cx+52,cy-29,cx+62,cy-29,cx+57,cy-23,0xF800);
        tft.fillCircle(cx+70,cy-40,2,0xF81F); tft.fillCircle(cx+73,cy-40,2,0xF81F);
        tft.fillTriangle(cx+68,cy-39,cx+75,cy-39,cx+71,cy-34,0xF81F);
    }
    if (frame==2||frame==3) {
        tft.setTextSize(1); tft.setTextColor(0x07FF,bgCol);
        tft.setCursor(cx+45,cy-35); tft.print(F("z"));
        tft.setTextSize(2); tft.setCursor(cx+52,cy-45); tft.print(F("z"));
        tft.setTextSize(1); tft.setCursor(cx+65,cy-55); tft.print(F("Z"));
    }
}

// ---- CLOCK SCREEN ----
void drawClockBG() {
    tft.fillScreen(0x0000);
    tft.fillRect(0,0,320,30,0x0015);
    tft.drawFastHLine(0,30,320,0x07FF);
    tft.setTextSize(2); tft.setTextColor(0x07FF,0x0015);
    tft.setCursor(70,8); tft.print(F("DIGITAL CLOCK"));
    tft.drawFastVLine(215,30,188,0x2945);
    tft.drawFastHLine(0,170,215,0x2945);
    tft.setTextSize(1); tft.setTextColor(0x7BEF,0x0000);
    tft.setCursor(222,65); tft.print(F("AM/PM"));
    tft.setCursor(222,145); tft.print(F("SEC"));
    drawModeBar(0);
    drawCat(0);
}

void updateClockScreen(MyTime t) {
    bool isPM; uint8_t hh12 = to12hr(t.hh, isPM);
    const char* days[]   = {"","SUN","MON","TUE","WED","THU","FRI","SAT"};
    const char* months[] = {"","JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};

    if (t.hh!=last_hh || t.mm!=last_mm) {
        tft.fillRect(0,31,215,88,0x0000);
        tft.setTextSize(7); tft.setTextColor(0x05FD,0x0000);
        tft.setCursor(5,38);
        if(hh12<10) tft.print('0'); tft.print(hh12); tft.print(':');
        if(t.mm<10) tft.print('0'); tft.print(t.mm);
        uint16_t amBg=(!isPM)?0x07FF:0x2945, pmBg=(isPM)?0xF800:0x2945;
        tft.fillRoundRect(222,40,90,20,4,amBg); tft.fillRoundRect(222,65,90,20,4,pmBg);
        tft.setTextSize(2);
        tft.setTextColor(0x0000,amBg); tft.setCursor(248,44); tft.print(F("AM"));
        tft.setTextColor(0x0000,pmBg); tft.setCursor(248,69); tft.print(F("PM"));
    }
    if (t.day!=val_time.day||t.month!=val_time.month||t.year!=val_time.year||t.hh!=last_hh) {
        tft.fillRect(0,120,215,50,0x0000);
        tft.setTextSize(2); tft.setTextColor(0xFD20,0x0000);
        tft.setCursor(5,125);
        if(t.month>=1&&t.month<=12) tft.print(months[t.month]);
        tft.print(' '); if(t.day<10) tft.print('0'); tft.print(t.day);
        tft.print(F(", ")); tft.print(t.year);
        tft.setTextColor(0xFFE0,0x0000); tft.setCursor(5,148);
        if(t.dow>=1&&t.dow<=7) tft.print(days[t.dow]);
    }
    if (t.ss!=last_ss) {
        tft.fillRect(222,148,90,66,0x0000);
        tft.setTextSize(5); tft.setTextColor(0xF81F,0x0000);
        tft.setCursor(222,158); if(t.ss<10) tft.print('0'); tft.print(t.ss);
        tft.fillRect(160,198,55,20,0x0000);
        tft.setTextSize(2); tft.setTextColor(0xFD20,0x0000);
        tft.setCursor(160,200); if(t.ss<10) tft.print('0'); tft.print(t.ss); tft.print(' ');
    }
    last_hh=t.hh; last_mm=t.mm; last_ss=t.ss;
}

// ---- AIR QUALITY SCREEN ----
void drawAirBG() {
    tft.fillScreen(0x0000);
    tft.fillRect(0,0,320,30,0x3000); tft.drawFastHLine(0,30,320,0xFD20);
    tft.setTextSize(1); tft.setTextColor(0x7BEF,0x0000);
    tft.setCursor(5,35); tft.print(F("GAS"));
    tft.setCursor(65,35); tft.print(F("VALUE"));
    tft.setCursor(130,35); tft.print(F("UNIT"));
    tft.setCursor(175,35); tft.print(F("STATUS"));
    tft.drawFastHLine(0,44,320,0x2945);
    tft.setTextSize(2); tft.setTextColor(0xFFFF,0x0000);
    tft.setCursor(5,50); tft.print(F("CO2"));
    tft.setCursor(5,78); tft.print(F("CO"));
    tft.setCursor(5,106); tft.print(F("NH3"));
    tft.setCursor(5,134); tft.print(F("NO2"));
    tft.setCursor(5,162); tft.print(F("BENZ"));
    tft.setCursor(5,190); tft.print(F("SMOK"));
    for(int y=72;y<=214;y+=28) tft.drawFastHLine(0,y,320,0x2945);
    drawModeBar(1);
}

void updateAirRow(int y,int val,const char* badge,uint16_t col){
    tft.fillRect(60,y,260,22,0x0000);
    tft.setTextSize(2); tft.setTextColor(col,0x0000); tft.setCursor(65,y); tft.print(val);
    tft.setTextSize(1); tft.setTextColor(0x7BEF,0x0000); tft.setCursor(130,y+6); tft.print(F("ppm"));
    uint16_t bg=(badge[0]=='S')?0x0320:(badge[0]=='W')?0x5220:0x6000;
    tft.fillRoundRect(175,y+1,55,18,4,bg);
    tft.setTextSize(1); tft.setTextColor(0xFFFF,bg); tft.setCursor(180,y+6); tft.print(badge);
}

void updateAirScreen(int raw){
    if(raw==last_raw) return; last_raw=raw;
    float r=raw/1023.0;
    int co2=(int)(400+r*1600),co=(int)(r*50),nh3=(int)(r*100);
    int no2=(int)(r*10),benz=(int)(r*20),smoke=(int)(r*100);
    int ap=constrain(map(raw,100,800,0,100),0,100);
    const char* st; uint16_t sc;
    if(ap<25){st="GOOD    ";sc=0x07E0;}
    else if(ap<50){st="MODERATE";sc=0xFFE0;}
    else if(ap<75){st="POOR    ";sc=0xFD20;}
    else{st="DANGER! ";sc=0xF800;}
    tft.fillRect(0,0,320,30,0x3000);
    tft.setTextSize(2);
    tft.setTextColor(0xFD20,0x3000); tft.setCursor(5,8);   tft.print(F("Air Quality:"));
    tft.setTextColor(sc,0x3000);     tft.setCursor(200,8); tft.print(st);
    updateAirRow(50, co2,  co2 <1000?"SAFE":"HIGH", co2 <1000?0x07E0:0xF800);
    updateAirRow(78, co,   co  <10  ?"SAFE":"HIGH", co  <10  ?0x07E0:0xF800);
    updateAirRow(106,nh3,  nh3 <25  ?"SAFE":"HIGH", nh3 <25  ?0x07E0:0xF800);
    updateAirRow(134,no2,  no2 <3   ?"SAFE":"HIGH", no2 <3   ?0x07E0:0xF800);
    updateAirRow(162,benz, benz<5   ?"SAFE":"HIGH", benz<5   ?0x07E0:0xF800);
    updateAirRow(190,smoke,smoke<30 ?"SAFE":"WARN", smoke<30 ?0x07E0:0xFFE0);
}

// ---- TEMP SCREEN ----
void drawTempBG(){
    tft.fillScreen(0x0000);
    tft.fillRect(0,0,320,30,0x000F); tft.drawFastHLine(0,30,320,0x07FF);
    tft.setTextSize(2); tft.setTextColor(0x07FF,0x000F);
    tft.setCursor(60,8); tft.print(F("TEMP & HUMIDITY"));
    tft.drawFastVLine(159,30,188,0x4208);
    tft.setTextSize(2);
    tft.setTextColor(0xFD20,0x0000); tft.setCursor(15,35);  tft.print(F("TEMP"));
    tft.setTextColor(0x07FF,0x0000); tft.setCursor(185,35); tft.print(F("HUMID"));
    tft.setTextColor(0xFFFF,0x0000);
    tft.setCursor(5,160); tft.print(F("Celsius"));
    tft.setCursor(220,160); tft.print(F("%"));
    drawModeBar(2);
}

void updateTempScreen(float temp,float humi){
    if(abs(temp-last_temp)>=0.1){
        last_temp=temp;
        uint16_t tCol=(temp<20)?0x001F:(temp<30)?0x07E0:(temp<35)?0xFFE0:0xF800;
        tft.fillRect(0,55,158,100,0x0000);
        tft.setTextSize(6); tft.setTextColor(tCol,0x0000); tft.setCursor(5,60); tft.print((int)temp);
        tft.setTextSize(3); tft.setCursor(85,90); tft.print('.'); tft.print((int)(temp*10)%10);
        tft.drawRect(5,178,148,16,0xFFFF); tft.fillRect(6,179,146,14,0x0000);
        tft.fillRect(6,179,map(constrain((int)temp,0,50),0,50,0,146),14,tCol);
        String ts; uint16_t tc;
        if(temp<20){ts=F("COLD  ");tc=0x001F;}
        else if(temp<25){ts=F("COOL  ");tc=0x07FF;}
        else if(temp<30){ts=F("NORMAL");tc=0x07E0;}
        else if(temp<35){ts=F("WARM  ");tc=0xFFE0;}
        else{ts=F("HOT!  ");tc=0xF800;}
        tft.fillRoundRect(5,197,148,22,5,0x2104);
        tft.setTextSize(2); tft.setTextColor(tc,0x2104); tft.setCursor(15,202); tft.print(ts);
    }
    if(abs(humi-last_humi)>=1.0){
        last_humi=humi;
        uint16_t hCol=(humi<30)?0xF800:(humi<60)?0x07E0:0x001F;
        tft.fillRect(161,55,158,100,0x0000);
        tft.setTextSize(6); tft.setTextColor(hCol,0x0000); tft.setCursor(165,60); tft.print((int)humi);
        tft.drawRect(162,178,153,16,0xFFFF); tft.fillRect(163,179,151,14,0x0000);
        tft.fillRect(163,179,map((int)humi,0,100,0,151),14,hCol);
        String hs; uint16_t hc;
        if(humi<30){hs=F("DRY   ");hc=0xF800;}
        else if(humi<60){hs=F("NORMAL");hc=0x07E0;}
        else if(humi<80){hs=F("HUMID ");hc=0x07FF;}
        else{hs=F("WET!  ");hc=0x001F;}
        tft.fillRoundRect(162,197,153,22,5,0x2104);
        tft.setTextSize(2); tft.setTextColor(hc,0x2104); tft.setCursor(172,202); tft.print(hs);
    }
}

// ============================================================
// EYE UPDATE LOOP (called every loop() when in eye desktop)
// ============================================================
void updateEyes() {
    unsigned long now = millis();

    // ---- Return to IDLE after expression timeout ----
    if (eyeState != ES_IDLE && eyeState != ES_SLEEPY && eyeDuration > 0) {
        if (now - eyeStateStart >= eyeDuration) {
            setEyeState(ES_IDLE, 0);
            drawIdle(pupX, pupY);
            nextBlink = now + 2000;
        }
        return; // don't do idle logic while in expression
    }

    // ---- Sleepy trigger ----
    if (eyeState == ES_IDLE && (now - lastInteraction) >= SLEEPY_MS) {
        setEyeState(ES_SLEEPY, 0);
        drawSleepy();
        return;
    }

    // ---- Auto blink in IDLE ----
    if (eyeState == ES_IDLE && now >= nextBlink) {
        // Closing phase
        for (int c = 0; c <= EH; c += 10) { drawBlink(c); delay(18); }
        // Opening phase
        for (int c = EH; c >= 0; c -= 10) { drawBlink(c); delay(18); }
        // Restore pupils
        drawIdle(pupX, pupY);
        nextBlink = now + (long)random(3000, 7000);
        return;
    }

    // ---- Smooth pupil drift in IDLE ----
    if (eyeState == ES_IDLE) {
        // Pick a new target occasionally
        if (now >= nextPupilMove) {
            pupTX = random(-11, 12);
            pupTY = random(-7, 8);
            nextPupilMove = now + (long)random(1200, 3500);
        }

        // Step toward target every 55ms
        if (now - lastPupilStep >= 55) {
            lastPupilStep = now;
            bool moved = false;
            int dx = pupTX - pupX;
            int dy = pupTY - pupY;
            if (abs(dx) > 1) { pupX += (dx > 0) ? 2 : -2; moved = true; }
            else if (dx != 0) { pupX = pupTX; moved = true; }
            if (abs(dy) > 1) { pupY += (dy > 0) ? 1 : -1; moved = true; }
            else if (dy != 0) { pupY = pupTY; moved = true; }

            if (moved) {
                // Erase old pupils (redraw eye whites under them)
                tft.fillRoundRect(LE_X, EY, EW, EH, ER, 0xFFFF);
                tft.fillRoundRect(RE_X, EY, EW, EH, ER, 0xFFFF);
                drawPupils(pupX, pupY);
            }
        }
    }

    // ---- Hearts animation in HAPPY state ----
    if (eyeState == ES_HAPPY) {
        if (now - heartTimer >= 300) {
            heartTimer = now;
            heartFrame = (heartFrame + 1) % 6;
            drawHappyHearts(heartFrame);
        }
    }
}

// ============================================================
// HANDLE EYE GESTURES (called when packet arrives)
// ============================================================
void handleEyeGesture() {
    if (val_btn) {
        // Single tap → surprised
        setEyeState(ES_SURPRISED, 1800);
        drawSurprised();
    }
    else if (val_dtap) {
        // Double tap → happy with hearts
        setEyeState(ES_HAPPY, 3500);
        heartFrame = 0;
        heartTimer = millis();
        drawHappyHearts(0);
    }
    else if (val_lbtn) {
        // Long press (600–1500ms) → angry
        setEyeState(ES_ANGRY, 2500);
        drawAngry();
    }
    else if (val_slbtn) {
        // Super long (>1500ms) → happy flash then exit to sensor desktop
        drawHappy();
        delay(800);
        currentDesktop = 0;
        resetCache();
        if      (currentMode==0) drawClockBG();
        else if (currentMode==1) drawAirBG();
        else                     drawTempBG();
    }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(A5));

    uint16_t lcdID = tft.readID();
    if (lcdID==0xD3D3||lcdID==0xFFFF||lcdID==0x0000) lcdID=0x9341;
    tft.begin(lcdID);
    tft.setRotation(1);
    tft.fillScreen(0x0000);

    // Boot animation on eye desktop
    bootAnimation();

    delay(300);

    // Enter sensor desktop (clock)
    drawClockBG();

    tft.setTextSize(1);
    tft.setTextColor(0x7BEF,0x0000);
    tft.setCursor(220,40);
    tft.print(F("Waiting..."));
}

// ============================================================
// LOOP
// ============================================================
void loop() {
    // ---- Read serial from Nano ----
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            if (serialBuffer.length() > 0) {
                parsePacket(serialBuffer);
                serialBuffer = "";

                if (currentDesktop == 0) {
                    // ---- Sensor desktop ----
                    if (val_lbtn) {
                        // Long press → switch to eye desktop
                        currentDesktop = 1;
                        enterEyeDesktop();
                    }
                    else if (val_btn) {
                        currentMode = (currentMode + 1) % 3;
                        resetCache();
                        if      (currentMode==0) drawClockBG();
                        else if (currentMode==1) drawAirBG();
                        else                     drawTempBG();
                    }
                    else {
                        // Update sensor screen
                        if      (currentMode==0) updateClockScreen(val_time);
                        else if (currentMode==1) updateAirScreen(val_raw);
                        else                     updateTempScreen(val_temp, val_humi);
                    }

                } else {
                    // ---- Eye desktop ----
                    handleEyeGesture();
                }
            }
        } else {
            serialBuffer += c;
            if (serialBuffer.length() > 220) serialBuffer = "";
        }
    }

    // ---- Cat animation (clock screen only) ----
    unsigned long now = millis();
    if (currentDesktop == 0 && currentMode == 0 && now - catMillis >= 500) {
        catMillis = now;
        catFrame = (catFrame + 1) % 4;
        drawCat(catFrame);
    }

    // ---- Eye idle animations ----
    if (currentDesktop == 1) {
        updateEyes();
    }
}
