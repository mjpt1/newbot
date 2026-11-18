// Test design D: spring cartoon eye (user-supplied code). Build env: eyespring
// Fixed vs. original: backlight pin 2 -> 40, and DC/CS order in the Adafruit constructor.
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// پین‌های استاندارد Waveshare ESP32-S3-LCD-1.28 برای نمایشگر GC9A01
#define TFT_CS   9
#define TFT_DC   8
#define TFT_RST  12
#define TFT_MOSI 11
#define TFT_SCLK 10
#define TFT_BL   40  // نور پس‌زمینه (روی این برد GPIO40 است، نه 2)

// ترتیب آرگومان‌ها در این کتابخانه: (spi, DC, CS, RST)
Adafruit_GC9A01A tft = Adafruit_GC9A01A(&SPI, TFT_DC, TFT_CS, TFT_RST);

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

// ساختار فیزیک فنر (Spring Physics) برای حرکت فوق‌العاده نرم و ارگانیک بدون سرگیجه
struct Spring {
    float pos;
    float target;
    float vel;
    float k;      // سختی فنر (Stiffness)
    float damp;   // میراگری (Damping)

    void update(float dt) {
        float force = -k * (pos - target) - damp * vel;
        vel += force * dt;
        pos += vel * dt;
    }

    void setTarget(float t) {
        target = t;
    }
};

// متغیرهای فیزیکی چشم
Spring eyeX = {0, 0, 0, 140.0f, 14.0f};
Spring eyeY = {0, 0, 0, 140.0f, 14.0f};
Spring eyelid = {0, 0, 0, 200.0f, 18.0f}; // فیزیک پلک

unsigned long lastUpdate = 0;
unsigned long nextActionTime = 0;
int currentMood = 0; // 0: عادی، 1: خمار/کنجکاو، 2: تعجب

void triggerNewAction() {
    int action = random(0, 4);
    if (action == 0 || action == 1) {
        // تغییر جهت نگاه نرم
        float newX = (random(-50, 51) / 50.0f) * 35.0f;
        float newY = (random(-40, 41) / 40.0f) * 25.0f;
        eyeX.setTarget(newX);
        eyeY.setTarget(newY);
        currentMood = 0;
    }
    else if (action == 2) {
        // پلک زدن عمیق
        eyelid.setTarget(1.0f);
        delay(80);
        eyelid.setTarget(0.0f);
    }
    else {
        // حالت تعجب یا خمار
        currentMood = random(1, 3);
        if (currentMood == 1) {
            eyeY.setTarget(15.0f); // خمار
        } else {
            eyeY.setTarget(-15.0f); // تعجب
        }
    }
    nextActionTime = millis() + random(1200, 3200);
}

void setup() {
    Serial.begin(115200);

    // راه‌اندازی پین نور پس‌زمینه
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // راه‌اندازی SPI و نمایشگر
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(GC9A01A_BLACK);

    randomSeed(analogRead(0));
    nextActionTime = millis() + 1000;
    lastUpdate = millis();
}

void loop() {
    unsigned long now = millis();
    float dt = (now - lastUpdate) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f; // جلوگیری از پرش زمانی
    lastUpdate = now;

    if (now >= nextActionTime) {
        triggerNewAction();
    }

    // به‌روزرسانی محاسبات فیزیک فنر
    eyeX.update(dt);
    eyeY.update(dt);
    eyelid.update(dt);

    // پاک کردن قاب قبلی (مشکی)
    tft.fillScreen(GC9A01A_BLACK);

    // مختصات مرکز چشم با احتساب فیزیک نرم
    int cx = CENTER_X + (int)eyeX.pos;
    int cy = CENTER_Y + (int)eyeY.pos;

    // اندازه پایه چشم Starboy
    int eyeW = 75;
    int eyeH = 85;
    if (currentMood == 1) eyeH = 55; // خمار
    if (currentMood == 2) { eyeW = 85; eyeH = 95; } // تعجب

    // اعمال فیزیک پلک روی ارتفاع چشم
    int currentH = (int)(eyeH * (1.0f - eyelid.pos * 0.9f));
    if (currentH < 4) currentH = 4;

    // ۱. رسم بدنه اصلی چشم (سفیدیِ تمیز و کارتونی)
    tft.fillRoundRect(cx - eyeW, cy - currentH, eyeW * 2, currentH * 2, currentH, GC9A01A_WHITE);

    // ۲. رسم مردمک مشکی بزرگ که به نرمی جابه‌جا میشه
    int pupilW = 28;
    int pupilH = 34;
    int px = cx + (int)(eyeX.pos * 0.3f);
    int py = cy + (int)(eyeY.pos * 0.3f);
    tft.fillRoundRect(px - pupilW, py - pupilH, pupilW * 2, pupilH * 2, pupilH, GC9A01A_BLACK);

    // ۳. های‌لایت درخشان (Sparkle) برای زنده شدن نگاه
    tft.fillCircle(cx - 20, cy - 25, 7, GC9A01A_WHITE);
    tft.fillCircle(cx - 14, cy - 19, 3, GC9A01A_WHITE);

    // کنترل فریم‌ریت برای روانی کامل (~۵۰ الی ۶۰ فریم بر ثانیه)
    delay(12);
}
