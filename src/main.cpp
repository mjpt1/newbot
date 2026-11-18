// RoboEyes for Waveshare ESP32-S3-LCD-1.28 (GC9A01 240x240 round, QMI8658 IMU)
// Hardware layer: display, two-core rendering, motion sensor, BOOT button,
// battery, WiFi web control, clock and over-the-air update.
// All drawing/behaviour lives in src/core (also runs on PC: tools/sim.cpp).

#include <Arduino.h>
#include <ESPmDNS.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#include "core/robot.h"
#include "web_page.h"

// ---------- pins (Waveshare ESP32-S3-LCD-1.28) ----------
static const int PIN_LCD_DC = 8, PIN_LCD_CS = 9, PIN_LCD_SCK = 10, PIN_LCD_MOSI = 11, PIN_LCD_RST = 12, PIN_LCD_BL = 40;
static const int PIN_IMU_SDA = 6, PIN_IMU_SCL = 7;
static const int PIN_BAT_ADC = 1;
static const float BAT_DIVIDER = 3.0f;  // board's resistor divider on the battery sense pin
static const int PIN_BUTTON = 0;         // BOOT button

static const char *AP_PASS = "robot1234";
static const char *DEFAULT_TZ = "<+0330>-3:30";  // Iran time

// ---------- display ----------
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 panel;
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM light;

 public:
  LGFX() {
    auto b = bus.config();
    b.spi_host = SPI2_HOST; b.spi_mode = 0; b.freq_write = 80000000;
    b.pin_sclk = PIN_LCD_SCK; b.pin_mosi = PIN_LCD_MOSI; b.pin_miso = -1; b.pin_dc = PIN_LCD_DC;
    b.dma_channel = SPI_DMA_CH_AUTO;
    bus.config(b); panel.setBus(&bus);
    auto p = panel.config();
    p.pin_cs = PIN_LCD_CS; p.pin_rst = PIN_LCD_RST; p.panel_width = 240; p.panel_height = 240; p.invert = true;
    panel.config(p);
    auto l = light.config();
    l.pin_bl = PIN_LCD_BL; l.freq = 44100; l.pwm_channel = 7;
    light.config(l); panel.setLight(&light);
    setPanel(&panel);
  }
};

static LGFX lcd;
static LGFX_Sprite bufA(&lcd), bufB(&lcd);
static LGFX_Sprite *drawBuf = &bufA;
static Robot robot;

// ---------- two-core rendering: core 1 draws the top half, core 0 the bottom half ----------
static SemaphoreHandle_t semStart, semDone;
static Canvas jobCanvas;

static void renderWorker(void *) {
  for (;;) {
    xSemaphoreTake(semStart, portMAX_DELAY);
    Canvas c = jobCanvas; c.y0 = 120; c.y1 = 240;
    robot.render(c);
    xSemaphoreGive(semDone);
  }
}

// ---------- QMI8658 motion sensor ----------
static uint8_t imuAddr = 0x6B;
static bool imuOk = false;

static void imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(imuAddr); Wire.write(reg); Wire.write(val); Wire.endTransmission();
}
static bool imuReadRegs(uint8_t reg, uint8_t *dst, int n) {
  Wire.beginTransmission(imuAddr); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)imuAddr, n) != n) return false;
  for (int i = 0; i < n; i++) dst[i] = Wire.read();
  return true;
}
static void imuBegin() {
  Wire.begin(PIN_IMU_SDA, PIN_IMU_SCL, 400000);
  for (uint8_t a : {0x6B, 0x6A}) {
    imuAddr = a;
    uint8_t id = 0;
    if (imuReadRegs(0x00, &id, 1) && id == 0x05) { imuOk = true; break; }
  }
  if (!imuOk) { Serial.println("QMI8658 not found"); return; }
  imuWrite(0x02, 0x40);  // CTRL1: register auto-increment
  imuWrite(0x03, 0x15);  // CTRL2: accel +-4g, ~235 Hz
  imuWrite(0x04, 0x55);  // CTRL3: gyro +-512 dps, ~235 Hz
  imuWrite(0x06, 0x11);  // CTRL5: low-pass filters on
  imuWrite(0x08, 0x03);  // CTRL7: accel + gyro on
  Serial.println("QMI8658 ready");
}
static void imuSample(Inputs &in) {
  if (!imuOk) return;
  uint8_t d[12];
  if (!imuReadRegs(0x35, d, 12)) return;
  auto s16 = [&](int i) { return (int16_t)(d[i] | (d[i + 1] << 8)); };
  in.imuOk = true;
  in.ax = s16(0) / 8192.0f; in.ay = s16(2) / 8192.0f; in.az = s16(4) / 8192.0f;
  float gx = s16(6) / 64.0f, gy = s16(8) / 64.0f, gz = s16(10) / 64.0f;
  in.gyro = sqrtf(gx * gx + gy * gy + gz * gz);
  static uint32_t lastLog = 0;
  if (millis() - lastLog > 100) {
    lastLog = millis();
    uint8_t ts[3] = {0, 0, 0};
    imuReadRegs(0x30, ts, 3);  // sample counter: must keep changing if the sensor is running
    Serial.printf("imu a=%.3f %.3f %.3f g=%.0f %.0f %.0f ts=%lu\n", in.ax, in.ay, in.az, gx, gy, gz,
                  (unsigned long)(ts[0] | (ts[1] << 8) | (ts[2] << 16)));
  }
}

// ---------- BOOT button: click / double click / long press ----------
static int readButton() {
  static bool was = false;
  static uint32_t downAt = 0, lastUp = 0;
  static bool longFired = false, pendingClick = false;
  static bool raw = false;
  static uint32_t rawAt = 0;
  uint32_t now = millis();
  bool r = digitalRead(PIN_BUTTON) == LOW;
  if (r != raw) { raw = r; rawAt = now; }
  bool down = (now - rawAt > 25) ? raw : was;  // 25 ms debounce
  int ev = 0;
  if (down && !was) { downAt = now; longFired = false; }
  if (down && !longFired && now - downAt > 900) { longFired = true; pendingClick = false; ev = 2; }
  if (!down && was && !longFired) {
    if (pendingClick && now - lastUp < 350) { pendingClick = false; ev = 3; }
    else { pendingClick = true; lastUp = now; }
  }
  if (pendingClick && !down && now - lastUp >= 350) { pendingClick = false; ev = 1; }
  was = down;
  if (ev) Serial.printf("button event %d\n", ev);
  return ev;
}

// ---------- battery ----------
static float readBattery() {
  static float v = 0;
  static uint32_t last = 0;
  if (millis() - last > 2000 || v == 0) {
    last = millis();
    float now = analogReadMilliVolts(PIN_BAT_ADC) * BAT_DIVIDER / 1000.0f;
    v = v == 0 ? now : v * 0.8f + now * 0.2f;
  }
  return v;
}

// ---------- WiFi + web ----------
static WebServer server(80);
static Preferences prefs;
static Inputs webIn;  // commands from the web page, consumed once per frame
static int brightness = 255;
static float eyeHue = -1;  // -1 = white eyes

static String stateJson(int emoOverride = -1) {
  char tm[8] = "";
  time_t now = time(nullptr);
  if (now > 1600000000) { struct tm lt; localtime_r(&now, &lt); strftime(tm, sizeof tm, "%H:%M", &lt); }
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  char b[220];
  snprintf(b, sizeof b, "{\"emo\":%d,\"hue\":%d,\"bri\":%d,\"clock\":%d,\"ip\":\"%s\",\"time\":\"%s\",\"batt\":%.2f}",
           emoOverride >= 0 ? emoOverride : (int)robot.face.emo, (int)eyeHue, brightness,
           (webIn.setClock >= 0 ? webIn.setClock : robot.clockMode) ? 1 : 0, ip.c_str(), tm, readBattery());
  return b;
}

static void webBegin() {
  prefs.begin("robot", false);
  eyeHue = -1;  // always start with white eyes (colour from the web lasts until restart)
  prefs.remove("hue");
  brightness = prefs.getInt("bri", 255);
  String ssid = prefs.getString("ssid", ""), pass = prefs.getString("pass", "");
  String tz = prefs.getString("tz", DEFAULT_TZ);

  char apName[24];
  snprintf(apName, sizeof apName, "RoboEyes-%04X", (uint16_t)(ESP.getEfuseMac() >> 32));
  WiFi.onEvent([](WiFiEvent_t ev, WiFiEventInfo_t info) {
    if (ev == ARDUINO_EVENT_WIFI_STA_GOT_IP) Serial.printf("WiFi home OK, IP %s\n", WiFi.localIP().toString().c_str());
    if (ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) Serial.printf("WiFi home failed, reason %d\n", info.wifi_sta_disconnected.reason);
  });
  Serial.printf("WiFi home ssid='%s' passlen=%d\n", ssid.c_str(), pass.length());
  WiFi.mode(WIFI_STA);
  int nNet = WiFi.scanNetworks();
  for (int i = 0; i < nNet; i++) Serial.printf("WiFi seen '%s' ch%d %ddBm\n", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
  WiFi.scanDelete();
  WiFi.mode(ssid.length() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(apName, AP_PASS);
  if (ssid.length()) WiFi.begin(ssid.c_str(), pass.c_str());
  configTzTime(tz.c_str(), "pool.ntp.org", "time.google.com");
  MDNS.begin("robot");

  server.on("/", [] { server.send_P(200, "text/html", WEB_PAGE); });
  server.on("/api/state", [] { server.send(200, "application/json", stateJson()); });
  server.on("/api/set", [] {
    if (server.hasArg("emo")) webIn.setEmotion = server.arg("emo").toInt();
    if (server.hasArg("hue")) { eyeHue = server.arg("hue").toFloat(); webIn.setHue = eyeHue; }
    if (server.hasArg("bri")) { brightness = constrain(server.arg("bri").toInt(), 10, 255); prefs.putInt("bri", brightness); }
    if (server.hasArg("clock")) webIn.setClock = server.arg("clock").toInt() ? 1 : 0;
    server.send(200, "application/json", stateJson(webIn.setEmotion));
  });
  server.on("/api/wifi", HTTP_POST, [] {
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("pass", server.arg("pass"));
    if (server.hasArg("tz") && server.arg("tz").length()) prefs.putString("tz", server.arg("tz"));
    server.send(200, "text/plain", "ok");
    delay(400);
    ESP.restart();
  });
  server.on("/update", HTTP_POST,
    [] {
      bool ok = !Update.hasError();
      server.send(200, "text/plain", ok ? "Done. Restarting..." : "Update failed");
      if (ok) { delay(500); ESP.restart(); }
    },
    [] {
      HTTPUpload &up = server.upload();
      if (up.status == UPLOAD_FILE_START) Update.begin(UPDATE_SIZE_UNKNOWN);
      else if (up.status == UPLOAD_FILE_WRITE) Update.write(up.buf, up.currentSize);
      else if (up.status == UPLOAD_FILE_END) Update.end(true);
    });
  server.begin();
  Serial.printf("WiFi AP %s (pass %s), http://192.168.4.1 or http://robot.local\n", apName, AP_PASS);
}

// ---------- clock text on top of the face ----------
static void drawClock(LGFX_Sprite &s) {
  time_t now = time(nullptr);
  char txt[8] = "--:--";
  if (now > 1600000000) { struct tm lt; localtime_r(&now, &lt); strftime(txt, sizeof txt, "%H:%M", &lt); }
  Col c = hsv(robot.face.hue, 0.35f * robot.face.tint.x, 1.0f);
  s.setFont(&fonts::FreeSansBold24pt7b);
  s.setTextDatum(textdatum_t::middle_center);
  s.setTextColor(s.color565((int)c.r, (int)c.g, (int)c.b));
  s.drawString(txt, 120, 172);
  if (now <= 1600000000) {
    s.setFont(&fonts::Font2);
    s.setTextColor(s.color565(120, 140, 135));
    s.drawString("connect WiFi", 120, 204);
  }
}

// ---------- main ----------
static uint32_t lastUs;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  analogReadResolution(12);

  lcd.init();
  lcd.setBrightness(0);
  for (LGFX_Sprite *b : {&bufA, &bufB}) {
    b->setColorDepth(16);
    b->setPsram(false);  // internal RAM: fast and DMA-friendly
    if (!b->createSprite(240, 240)) { b->setPsram(true); b->createSprite(240, 240); }
    b->fillScreen(TFT_BLACK);
  }
  lcd.startWrite();
  lcd.pushImageDMA(0, 0, 240, 240, (lgfx::swap565_t *)bufA.getBuffer());

  rng.s ^= esp_random();
  imuBegin();
  webBegin();
  robot.begin(true);
  if (eyeHue >= 0) { robot.face.hue.snap(eyeHue); robot.face.tint.snap(1); }
  lcd.setBrightness(brightness);

  semStart = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(renderWorker, "render", 6144, nullptr, 3, nullptr, 0);
  lastUs = micros();
}

void loop() {
  server.handleClient();

  uint32_t nowUs = micros();
  float dt = (nowUs - lastUs) / 1e6f;
  lastUs = nowUs;
  if (dt > 0.1f) dt = 0.1f;

  Inputs in = webIn;
  webIn = Inputs();
  imuSample(in);
  in.button = readButton();
  in.battV = readBattery();
  robot.update(dt, in);

  // render both halves at the same time on the two cores
  jobCanvas.buf = (uint16_t *)drawBuf->getBuffer();
  xSemaphoreGive(semStart);
  Canvas top = jobCanvas; top.y0 = 0; top.y1 = 120;
  uint32_t t0 = micros();
  robot.render(top);
  uint32_t t1 = micros();
  xSemaphoreTake(semDone, portMAX_DELAY);
  uint32_t t2 = micros();
  static uint32_t accTop = 0, accWait = 0, accPush = 0, accWaitDma = 0;
  accTop += t1 - t0; accWait += t2 - t1;

  if (!robot.booting && robot.clockMode && robot.face.faceScale < 0.8f) drawClock(*drawBuf);

  lcd.setBrightness(robot.lowBattery ? min(brightness, 70) : brightness);
  uint32_t t3 = micros();
  lcd.waitDMA();
  uint32_t t4 = micros();
  lcd.pushImageDMA(0, 0, 240, 240, (lgfx::swap565_t *)drawBuf->getBuffer());
  accPush += micros() - t4;
  accWaitDma += t4 - t3;
  drawBuf = (drawBuf == &bufA) ? &bufB : &bufA;

  static uint32_t fpsT = 0, frames = 0;
  frames++;
  if (millis() - fpsT > 2000) {
    Serial.printf("fps %.1f btn=%d emo=%d\n", frames * 1000.0f / (millis() - fpsT), digitalRead(PIN_BUTTON), (int)robot.face.emo);
    Serial.printf("  ms/frame top=%.1f wait0=%.1f push=%.1f waitDMA=%.1f\n", accTop / 1000.0f / frames, accWait / 1000.0f / frames,
                  accPush / 1000.0f / frames, accWaitDma / 1000.0f / frames);
    accTop = accWait = accPush = accWaitDma = 0;
    fpsT = millis(); frames = 0;
  }
}
