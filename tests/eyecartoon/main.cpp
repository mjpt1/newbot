// Test design C: cartoon Starboy eye (user-supplied code). Build env: eyecartoon
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

// پیکربندی نمایشگر
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft); // فریم‌بافر برای اجرای کاملاً روان و بدون پرش (Flicker-free)

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

// متغیرهای حالت چشم Starboy
float targetOffsetX = 0.0f;
float targetOffsetY = 0.0f;
float currentOffsetX = 0.0f;
float currentOffsetY = 0.0f;

// حالت‌های احساسی و پلک
float blinkProgress = 0.0f; // 0.0 باز، 1.0 کاملاً بسته
bool isBlinking = false;
unsigned long nextBlinkTime = 0;
unsigned long nextExpressionTime = 0;

// استایل چشم: 0 مخروطی/گرد عادی، 1 خمار/متفکر، 2 تعجب/گرد بزرگ
int eyeMood = 0;

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void chooseNewExpression() {
    int choice = random(0, 4);
    if (choice == 0) {
        eyeMood = 0; // عادی
    } else if (choice == 1) {
        eyeMood = 1; // کنجکاو / خمار
    } else if (choice == 2) {
        eyeMood = 2; // تعجب / هیجان‌زده
    } else {
        eyeMood = 0;
    }

    // جهت نگاه تصادفی به سبک Starboy
    targetOffsetX = (random(-40, 41) / 100.0f) * 45.0f;
    targetOffsetY = (random(-30, 31) / 100.0f) * 35.0f;

    nextExpressionTime = millis() + random(1800, 4000);
}

void updateBlinkLogic() {
    unsigned long now = millis();
    if (!isBlinking && now >= nextBlinkTime) {
        isBlinking = true;
        blinkProgress = 0.0f;
    }

    if (isBlinking) {
        blinkProgress += 0.15f; // سرعت بسته و باز شدن پلک
        if (blinkProgress >= 3.14159f) {
            isBlinking = false;
            blinkProgress = 0.0f;
            nextBlinkTime = now + random(2500, 5500);
        }
    }
}

void renderStarboyEye() {
    // درون‌یابی نرم حرکت چشم (Lerp)
    currentOffsetX += (targetOffsetX - currentOffsetX) * 0.2f;
    currentOffsetY += (targetOffsetY - currentOffsetY) * 0.2f;

    // پاکسازی پس‌زمینه با رنگ مشکی عمیق OLED
    spr.fillSprite(TFT_BLACK);

    // رسم هاله یا پس‌زمینه کهکشانی ملایم (اختیاری برای حس Starboy)
    for (int r = 115; r > 90; r--) {
        int alphaVal = map(r, 90, 115, 0, 30);
        spr.drawCircle(CENTER_X, CENTER_Y, r, rgb565(alphaVal, alphaVal / 2, alphaVal * 2));
    }

    // مختصات مرکز کره چشم کاراکتر
    float eyeX = CENTER_X + currentOffsetX;
    float eyeY = CENTER_Y + currentOffsetY;

    // اندازه چشم بر اساس مود احساسی
    int baseEyeWidth = 75;
    int baseEyeHeight = 85;
    if (eyeMood == 1) {
        baseEyeHeight = 50; // خمار
    } else if (eyeMood == 2) {
        baseEyeWidth = 85;  // تعجب
        baseEyeHeight = 95;
    }

    // اعمال فیزیک پلک زدن روی ارتفاع چشم
    float blinkFactor = isBlinking ? sinf(blinkProgress) : 0.0f;
    int currentHeight = (int)(baseEyeHeight * (1.0f - blinkFactor * 0.95f));

    // ۱. بدنه اصلی چشم (سفیدیِ درخشان و یکدست به سبک کاراکترهای دیجیتال)
    // رسم بیضی یا دایره توپر سفید
    for (int y = -currentHeight; y <= currentHeight; y++) {
        int xSpan = (int)(baseEyeWidth * sqrtf(max(0.0f, 1.0f - (float)(y * y) / (currentHeight * currentHeight))));
        for (int x = -xSpan; x <= xSpan; x++) {
            int px = (int)eyeX + x;
            int py = (int)eyeY + y;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                spr.drawPixel(px, py, TFT_WHITE);
            }
        }
    }

    // ۲. مردمک مشکی بزرگ و جذاب (Pupil) که جهت نگاه را دنبال می‌کند
    float pupilOffsetX = currentOffsetX * 0.35f;
    float pupilOffsetY = currentOffsetY * 0.35f;
    int pupilW = 32;
    int pupilH = 38;
    if (eyeMood == 1) pupilH = 24;

    for (int y = -pupilH; y <= pupilH; y++) {
        int xSpan = (int)(pupilW * sqrtf(max(0.0f, 1.0f - (float)(y * y) / (pupilH * pupilH))));
        for (int x = -xSpan; x <= xSpan; x++) {
            int px = (int)eyeX + (int)pupilOffsetX + x;
            int py = (int)eyeY + (int)pupilOffsetY + y;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                spr.drawPixel(px, py, TFT_BLACK);
            }
        }
    }

    // ۳. های‌لایت براق و زنده (Glassy Reflection Sparkle) - ویژگی اصلی نگاه‌های کارتونیک
    int sparkX = (int)eyeX - 22;
    int sparkY = (int)eyeY - 28;
    spr.fillCircle(sparkX, sparkY, 7, TFT_WHITE);
    spr.fillCircle(sparkX + 6, sparkY + 6, 3, TFT_WHITE);

    // ارسال فریم به نمایشگر گرد GC9A01
    spr.pushSprite(0, 0);
}

void setup() {
    Serial.begin(115200);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    spr.setColorDepth(16);
    if (spr.createSprite(SCREEN_W, SCREEN_H) == nullptr) {
        Serial.println("Sprite allocation failed!");
        while (1) delay(100);
    }

    randomSeed(analogRead(0));
    nextBlinkTime = millis() + 1500;
    nextExpressionTime = millis() + 1000;
}

void loop() {
    if (millis() >= nextExpressionTime) {
        chooseNewExpression();
    }

    updateBlinkLogic();
    renderStarboyEye();

    delay(12); // نرخ فریم بالا و روان (حدود ۶۰ فریم بر ثانیه)
}
