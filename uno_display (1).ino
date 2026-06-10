// ============================================================
// ARDUINO UNO — DISPLAY NODE
// TFT Shield: MCUFRIEND (ILI9341), pins 2–9, A0–A4
// Nano TX → Uno RX (pin 0)
//
// TAP  = cycle screens (Clock / Air / Temp)
// ============================================================

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

// ============================================================
// GLOBAL STATE
// ============================================================
MCUFRIEND_kbv tft;

int currentMode = 0;   // 0=Clock  1=Air  2=Temp

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
// MODE BAR
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

// ============================================================
// CAT ANIMATION
// ============================================================
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

// ============================================================
// CLOCK SCREEN
// ============================================================
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

// ============================================================
// AIR QUALITY SCREEN
// ============================================================
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

// ============================================================
// TEMP SCREEN
// ============================================================
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

                if (val_btn) {
                    currentMode = (currentMode + 1) % 3;
                    resetCache();
                    if      (currentMode==0) drawClockBG();
                    else if (currentMode==1) drawAirBG();
                    else                     drawTempBG();
                } else {
                    if      (currentMode==0) updateClockScreen(val_time);
                    else if (currentMode==1) updateAirScreen(val_raw);
                    else                     updateTempScreen(val_temp, val_humi);
                }
            }
        } else {
            serialBuffer += c;
            if (serialBuffer.length() > 220) serialBuffer = "";
        }
    }

    // ---- Cat animation (clock screen only) ----
    unsigned long now = millis();
    if (currentMode == 0 && now - catMillis >= 500) {
        catMillis = now;
        catFrame = (catFrame + 1) % 4;
        drawCat(catFrame);
    }
}
