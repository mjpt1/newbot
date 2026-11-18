// Test design B: single 3D eyeball (user-supplied code). Build env: eye3d
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

// پیکربندی نمایشگر
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft); // استفاده از اسپرایت ۱۶ بیتی در DMA/RAM برای حذف لرزش

// مشخصات هندسی نمایشگر گرد GC9A01
#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120
#define EYE_RADIUS 110

// ساختار بردار ۳بعدی برای جهت نگاه (Gaze Vector)
struct Vector3D {
    float x;
    float y;
    float z;
};

// متغیرهای حالت چشم
Vector3D currentGaze = {0.0f, 0.0f, 1.0f};
Vector3D targetGaze  = {0.0f, 0.0f, 1.0f};

// متغیرهای پلک زدن (Blinking)
float eyelidUpper = 0.0f; // 0.0 کاملاً باز, 1.0 کاملاً بسته
float eyelidLower = 0.0f;
bool isBlinking = false;
unsigned long nextBlinkTime = 0;
unsigned long blinkStartTime = 0;
const unsigned long blinkDuration = 180; // مدت زمان پلک زدن به میلی‌ثانیه

// زمان‌بندی تغییر جهت نگاه تصادفی
unsigned long nextSaccadeTime = 0;

// تبدیل رنگ ۲۴ بیتی RGB به ۱۶ بیتی RGB565 با بهینه‌سازی شیفت بیتی
inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// تابع نگاشت رنگ با ضریب روشنایی (Lighting Multiplier)
inline uint16_t applyLighting(uint8_t r, uint8_t g, uint8_t b, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    uint8_t nr = (uint8_t)(r * factor);
    uint8_t ng = (uint8_t)(g * factor);
    uint8_t nb = (uint8_t)(b * factor);
    return rgb565(nr, ng, nb);
}

// نرمال‌سازی بردار ۳بعدی
Vector3D normalize(Vector3D v) {
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 0.0001f) {
        v.x /= length;
        v.y /= length;
        v.z /= length;
    }
    return v;
}

// ضرب داخلی دو بردار (Dot Product)
inline float dotProduct(Vector3D a, Vector3D b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// انتخاب جهت تصادفی برای نگاه چشم (Saccadic Eye Movement)
void triggerSaccade() {
    float angle = random(0, 360) * (PI / 180.0f);
    float magnitude = (random(0, 100) / 100.0f) * 0.65f; // حداکثر ۶۵ درصد انحراف از مرکز

    targetGaze.x = cosf(angle) * magnitude;
    targetGaze.y = sinf(angle) * magnitude;
    targetGaze.z = sqrtf(max(0.0f, 1.0f - (targetGaze.x * targetGaze.x + targetGaze.y * targetGaze.y)));
    targetGaze = normalize(targetGaze);

    nextSaccadeTime = millis() + random(1200, 3500); // زمان تصادفی تا حرکت بعدی
}

// مدیریت انیمیشن پلک زدن
void updateBlink() {
    unsigned long now = millis();
    if (!isBlinking && now >= nextBlinkTime) {
        isBlinking = true;
        blinkStartTime = now;
    }

    if (isBlinking) {
        float progress = (float)(now - blinkStartTime) / (float)blinkDuration;
        if (progress >= 1.0f) {
            isBlinking = false;
            eyelidUpper = 0.0f;
            eyelidLower = 0.0f;
            nextBlinkTime = now + random(2500, 6000);
        } else {
            // انیمیشن نرم سینوسی برای باز و بسته شدن پلک
            float blinkFactor = sinf(progress * PI);
            eyelidUpper = blinkFactor;
            eyelidLower = blinkFactor * 0.35f; // پلک پایین حرکت محدودتری دارد
        }
    }
}

// هسته رندرینگ سه‌بعدی چشم روی فریم‌بافر
void renderEye() {
    // درون‌یابی نرم جهت نگاه (Lerp Smoothing)
    currentGaze.x += (targetGaze.x - currentGaze.x) * 0.18f;
    currentGaze.y += (targetGaze.y - currentGaze.y) * 0.18f;
    currentGaze.z += (targetGaze.z - currentGaze.z) * 0.18f;
    currentGaze = normalize(currentGaze);

    // منبع نور ثابت در فضا برای ایجاد انعکاس Specular (بالا سمت راست)
    Vector3D lightSource = normalize({-0.45f, -0.55f, 0.70f});

    // پاکسازی با رنگ مشکی حاشیه
    spr.fillSprite(TFT_BLACK);

    // پیمایش خطوط و ستون‌ها در محدوده کره چشم
    for (int y = -EYE_RADIUS; y <= EYE_RADIUS; y++) {
        int screenY = CENTER_Y + y;
        float yRel = (float)y / EYE_RADIUS;

        // محاسبه حدود پلک‌ها در راستای افقی
        float upperLimit = -1.0f + (eyelidUpper * 2.05f);
        float lowerLimit =  1.0f - (eyelidLower * 1.80f);

        // اگر نقطه زیر پلک پنهان بود، نیاز به رندر کره نیست
        if (yRel < upperLimit || yRel > lowerLimit) {
            continue;
        }

        // محاسبه پهنای افقی کره در ارتفاع y (معادله مقطع دایره)
        int xSpan = (int)sqrtf(max(0.0f, (float)(EYE_RADIUS * EYE_RADIUS - y * y)));

        for (int x = -xSpan; x <= xSpan; x++) {
            int screenX = CENTER_X + x;
            float xRel = (float)x / EYE_RADIUS;

            // محاسبه بردار نرمال سطح کره در نقطه (x, y)
            float zSq = 1.0f - (xRel * xRel + yRel * yRel);
            if (zSq < 0.0f) continue;
            float zRel = sqrtf(zSq);

            Vector3D surfaceNormal = {xRel, yRel, zRel};

            // محاسبه زاویه نقطه نسبت به محور نگاه مردمک (زاویه فضایی)
            float cosAngle = dotProduct(surfaceNormal, currentGaze);

            // سایه‌زنی پایه کره (Diffuse Shading) + سایه لبه‌ها (Rim Falloff)
            float diffuse = dotProduct(surfaceNormal, lightSource);
            diffuse = max(0.12f, diffuse); // روشنایی محیطی (Ambient)

            // محاسبه انعکاس نور (Specular Highlight) روی شیشه قرنیه
            // R = 2*(N.L)*N - L
            float nDotL = dotProduct(surfaceNormal, lightSource);
            float spec = 0.0f;
            if (nDotL > 0.0f) {
                Vector3D reflectDir = {
                    2.0f * nDotL * surfaceNormal.x - lightSource.x,
                    2.0f * nDotL * surfaceNormal.y - lightSource.y,
                    2.0f * nDotL * surfaceNormal.z - lightSource.z
                };
                reflectDir = normalize(reflectDir);
                // دید بیننده در راستای Z مثبت است
                float viewDotReflect = max(0.0f, reflectDir.z);
                spec = powf(viewDotReflect, 32.0f); // درخشش تیز های‌لایت
            }

            uint16_t pixelColor;

            // دسته‌بندی بافت چشم: مردمک، عنبیه، یا سفیدی چشم (Sclera)
            if (cosAngle > 0.945f) {
                // ۱. مردمک سیاه مرکزی (Pupil)
                float pupilEdge = (cosAngle - 0.945f) / 0.055f;
                uint8_t pVal = (uint8_t)(10 * pupilEdge);
                pixelColor = rgb565(pVal, pVal, pVal);
            }
            else if (cosAngle > 0.78f) {
                // ۲. عنبیه سایبرنتیک / ارگانیک (Iris) - تونالیته آبی فیروزه‌ای عمیق
                float irisPos = (cosAngle - 0.78f) / (0.945f - 0.78f);

                // بافت شیاردار شعاعی برای طبیعی‌تر شدن عنبیه
                float angleRad = atan2f(surfaceNormal.y - currentGaze.y, surfaceNormal.x - currentGaze.x);
                float ringTexture = 0.85f + 0.15f * sinf(angleRad * 24.0f);

                uint8_t r = (uint8_t)((18 + 40 * irisPos) * ringTexture);
                uint8_t g = (uint8_t)((120 + 110 * irisPos) * ringTexture);
                uint8_t b = (uint8_t)((190 + 65 * irisPos) * ringTexture);

                pixelColor = applyLighting(r, g, b, diffuse * 1.1f);
            }
            else {
                // ۳. صلبیه / سفیدی چشم (Sclera) با افکت عمق سایه در گوشه‌ها
                float edgeShadow = powf(zRel, 0.45f); // تیرگی طبیعی در افق کره
                uint8_t baseWhite = (uint8_t)(220 * edgeShadow);
                pixelColor = applyLighting(baseWhite, baseWhite - 8, baseWhite - 4, diffuse);
            }

            // ادغام های‌لایت درخشان قرنیه (Corneal Specular Reflection)
            if (spec > 0.35f) {
                uint8_t highlight = (uint8_t)(255 * spec);
                // ترکیب با رنگ زیرین
                pixelColor = rgb565(
                    min(255, ((pixelColor >> 11) << 3) + highlight),
                    min(255, (((pixelColor >> 5) & 0x3F) << 2) + highlight),
                    min(255, ((pixelColor & 0x1F) << 3) + highlight)
                );
            }

            spr.drawPixel(screenX, screenY, pixelColor);
        }
    }

    // ارسال فریم رندرشده به کنترلر GC9A01
    spr.pushSprite(0, 0);
}

void setup() {
    Serial.begin(115200);

    // راه‌اندازی نمایشگر
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    // تخصیص بافر اسپرایت (در صورت کمبود حافظه در SRAM داخلی، در PSRAM تعریف می‌شود)
    spr.setColorDepth(16);
    if (spr.createSprite(SCREEN_W, SCREEN_H) == nullptr) {
        Serial.println("Memory Allocation Failed for Full Sprite!");
        while (1) delay(100);
    }

    randomSeed(analogRead(0));
    nextBlinkTime = millis() + 2000;
    nextSaccadeTime = millis() + 1000;
}

void loop() {
    // ۱. به‌روزرسانی هوش حرکتی نگاه (تغییر فوکوس چشم به اطراف)
    if (millis() >= nextSaccadeTime) {
        triggerSaccade();
    }

    // ۲. پردازش فیزیک پلک زدن
    updateBlink();

    // ۳. رندر فریم سه‌بعدی
    renderEye();

    // فریم‌ریت کنترل‌شده (حدود ۴۵ الی ۵۰ فریم بر ثانیه)
    delay(5);
}
