// ============================================================
//  main.cpp  —  ESP32S3-CyberDesk for ESP32-S3
//               ST7789V 2.4" | XPT2046 Touch | WS2812 LED
//               Custom Wallpaper & Animated GIF Support
//               WiFi/NTP | OpenWeatherMap | Web Portal | BLE
//
//  Architecture:
//    Core 0 (loop): Display rendering, touch, animation, GIF
//    Core 1 (task): WiFi, NTP, weather fetch, Web Portal, BLE
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <sys/time.h>

#include "config.h"
#include "colors.h"
#include "led_ctrl.h"
#include "weather.h"
#include "ble_ctrl.h"
#include "touch_ctrl.h"
#include "wallpaper_mgr.h"
#include "web_portal.h"
#include "pages.h"

// ── Globals ──────────────────────────────────────────────────
TFT_eSPI     tft;
LedCtrl      led;
Weather      weather;
BleCtrl      ble;
TouchCtrl    touch;
WallpaperMgr wallpaper;
WebPortal    webPortal;
Preferences  prefs;

WiFiUDP      udp;
NTPClient    ntpClient(udp, NTP_SERVER1, NTP_UTC_OFFSET, 60000);

// Pages
ClockPage     clockPage;
WeatherPage   weatherPage;
RGBPage       rgbPage;
WallpaperPage wallpaperPage;
StatsPage     statsPage;

// ── State ─────────────────────────────────────────────────────
volatile uint8_t  currentPage    = PAGE_CLOCK;
volatile bool     pageChanged    = true;
volatile bool     bleConnected   = false;
volatile bool     ntpSynced      = false;
char              ipAddr[20]     = "0.0.0.0";

// ── Backlight PWM ────────────────────────────────────────────
uint8_t  blBrightness = BL_DEFAULT_BRIGHTNESS;

void setBacklight(uint8_t val) {
    blBrightness = val;
    ledcWrite(BL_PWM_CHANNEL, val);
}

// ── Compile-time RTC fallback ────────────────────────────────
void setCompileTimeFallback() {
    char s_month[5];
    int month, day, year;
    int hour, min, sec;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf(__DATE__, "%s %d %d", s_month, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

    month = (strstr(month_names, s_month) - month_names) / 3;

    struct tm t;
    t.tm_year = year - 1900;
    t.tm_mon  = month;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_sec  = sec;
    t.tm_isdst = -1;

    time_t epoch = mktime(&t);
    epoch += NTP_UTC_OFFSET;

    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    Serial.printf("[RTC] Fallback set to compile time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  year, month + 1, day, hour, min, sec);
}

// ── WiFi startup splash ──────────────────────────────────────
void drawWifiSplash(const char* msg, uint16_t color = C_CYAN) {
    tft.fillScreen(C_BG);
    drawCyberpunkBorder(tft, color);
    tft.setTextColor(C_MAGENTA);
    tft.setTextSize(2);
    tft.setCursor(20, 36);
    tft.println("CYBER");
    tft.setCursor(20, 56);
    tft.println("DESK S3");
    tft.drawFastHLine(10, 78, SCREEN_W - 20, C_CYAN);
    tft.setTextColor(color);
    tft.setTextSize(1);
    tft.setCursor(14, 88);
    tft.print(msg);

    static uint8_t dot = 0;
    for (int i = 0; i < 3; i++) {
        uint16_t dc = (i <= (dot % 3)) ? C_CYAN : C_DARK_GREY;
        tft.fillCircle(80 + i * 20, 114, 4, dc);
    }
    dot++;
}

// ── Touch Interactive Calibration ────────────────────────────
void runTouchCalibration() {
    uint16_t calData[5];
    tft.fillScreen(C_BG);
    drawCyberpunkBorder(tft, C_CYAN);
    tft.setTextColor(C_CYAN, C_BG);
    tft.setTextSize(2);
    tft.setCursor(24, 70);
    tft.println("TOUCH");
    tft.setCursor(24, 90);
    tft.println("CALIBRATION");

    tft.setTextColor(C_WHITE, C_BG);
    tft.setTextSize(1);
    tft.setCursor(16, 120);
    tft.println(">> Touch 4 corner crosshairs");
    tft.setCursor(16, 136);
    tft.println(">> Use a finger or stylus");
    tft.setTextColor(C_YELLOW, C_BG);
    tft.setCursor(16, 156);
    tft.println(">> Hold firmly on each target");

    tft.calibrateTouch(calData, C_MAGENTA, C_BG, 15);

    tft.fillScreen(C_BG);
    drawCyberpunkBorder(tft, C_GREEN);
    tft.setTextColor(C_GREEN, C_BG);
    tft.setTextSize(2);
    tft.setCursor(32, 130);
    tft.println("SUCCESS!");
    tft.setTextSize(1);
    tft.setCursor(24, 160);
    tft.setTextColor(C_WHITE, C_BG);
    tft.println("Calibration saved to flash");

    Serial.printf("[TOUCH] Saved calData: { %d, %d, %d, %d, %d }\n",
        calData[0], calData[1], calData[2], calData[3], calData[4]);

    prefs.putBytes("touchCal", calData, sizeof(calData));
    touch.setCalibration(calData);
    delay(1000);
    pageChanged = true;
}

// ── BLE command handler ───────────────────────────────────────
void handleBleCommand(const String& cmd) {
    Serial.printf("[BLE] cmd: %s\n", cmd.c_str());

    JsonDocument doc;
    if (deserializeJson(doc, cmd) != DeserializationError::Ok) return;

    const char* c = doc["cmd"] | "";

    if (strcmp(c, "page") == 0) {
        uint8_t p = doc["val"] | 0;
        if (p < NUM_PAGES) {
            currentPage = p;
            pageChanged = true;
        }
    } else if (strcmp(c, "led_on") == 0) {
        led.setEnabled(doc["val"] | true);
        rgbPage.markDirty();
        clockPage.markDirty();
    } else if (strcmp(c, "led_color") == 0) {
        led.setColor(doc["r"] | 0, doc["g"] | 200, doc["b"] | 255);
        rgbPage.markDirty();
    } else if (strcmp(c, "led_brightness") == 0) {
        led.setBrightness(doc["val"] | 100);
        rgbPage.markDirty();
    } else if (strcmp(c, "led_mode") == 0) {
        led.setMode(doc["val"] | 0);
        rgbPage.markDirty();
    } else if (strcmp(c, "wallpaper_mode") == 0) {
        uint8_t m = doc["val"] | 0;
        wallpaper.setMode((WallpaperMode)m);
        clockPage.markDirty();
        wallpaperPage.markDirty();
    } else if (strcmp(c, "display_brightness") == 0) {
        setBacklight(doc["val"] | BL_DEFAULT_BRIGHTNESS);
        prefs.putUChar("bl_brightness", blBrightness);
    } else if (strcmp(c, "unit") == 0) {
        const char* u = doc["val"] | "C";
        weather.setCelsius(strcmp(u, "C") == 0);
        weatherPage.markDirty();
        prefs.putBool("celsius", weather.isCelsius());
    } else if (strcmp(c, "weather_refresh") == 0) {
        weather.fetch();
        weatherPage.markDirty();
    } else if (strcmp(c, "status") == 0) {
        JsonDocument resp;
        resp["page"]     = currentPage;
        resp["led_on"]   = led.isEnabled();
        resp["led_r"]    = led.getR();
        resp["led_g"]    = led.getG();
        resp["led_b"]    = led.getB();
        resp["led_bri"]  = led.getBrightness();
        resp["led_mode"] = led.getMode();
        resp["wall_mode"]= (uint8_t)wallpaper.getMode();
        resp["bl"]       = blBrightness;
        resp["celsius"]  = weather.isCelsius();
        resp["wifi"]     = (WiFi.status() == WL_CONNECTED);
        resp["ip"]       = ipAddr;
        resp["temp"]     = weather.data().temp;
        String out;
        serializeJson(resp, out);
        ble.sendStatus(out);
    }

    prefs.putBool("led_on",  led.isEnabled());
    prefs.putUChar("led_r",  led.getR());
    prefs.putUChar("led_g",  led.getG());
    prefs.putUChar("led_b",  led.getB());
    prefs.putUChar("led_bri",led.getBrightness());
    prefs.putUChar("led_mode",led.getMode());
}

// ── Network / Web Portal / BLE task (Core 1) ──────────────────
void networkTask(void* param) {
    bool connected = false;
    if (strlen(WIFI_SSID_DEFAULT) > 0 && strcmp(WIFI_SSID_DEFAULT, "YOUR_WIFI_SSID") != 0) {
        Serial.printf("[WIFI] Connecting to '%s'...\n", WIFI_SSID_DEFAULT);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);

        uint32_t startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < 9000)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
        }

        connected = (WiFi.status() == WL_CONNECTED);
    }

    if (!connected) {
        Serial.println("\n[WIFI] Default WiFi connect failed, starting portal...");
        WiFiManager wm;
        wm.setConnectTimeout(WIFI_TIMEOUT_SEC);
        wm.setConfigPortalTimeout(60);
        connected = wm.autoConnect(WIFI_PORTAL_SSID, WIFI_PORTAL_PASS);
    }

    if (connected) {
        strlcpy(ipAddr, WiFi.localIP().toString().c_str(), sizeof(ipAddr));
        Serial.printf("\n[WIFI] Connected! IP: %s\n", ipAddr);

        // NTP sync
        ntpClient.begin();
        ntpClient.setTimeOffset(NTP_UTC_OFFSET);
        ntpClient.forceUpdate();
        ntpSynced = true;
        Serial.printf("[NTP] Synced time: %s, Epoch: %lu\n",
            ntpClient.getFormattedTime().c_str(), ntpClient.getEpochTime());

        // Weather fetch
        weather.fetch();
        weatherPage.markDirty();
        clockPage.markDirty();
    } else {
        strlcpy(ipAddr, "192.168.4.1", sizeof(ipAddr));
        Serial.println("\n[WIFI] Continuing in AP mode (192.168.4.1).");
    }

    // ── Start Cyberpunk Web Portal ──
    webPortal.begin(&wallpaper);

    // ── Start BLE ──
    ble.begin(handleBleCommand);

    // ── Network loop ──
    for (;;) {
        // Handle web upload requests
        webPortal.handleClient();

        if (WiFi.status() == WL_CONNECTED) {
            ntpClient.update();
            if (weather.needsUpdate()) {
                weather.fetch();
                weatherPage.markDirty();
            }
        }

        // Send BLE heartbeat every 5s
        static uint32_t bleHeartbeat = 0;
        if (millis() - bleHeartbeat > 5000) {
            bleHeartbeat = millis();
            bleConnected = ble.isConnected();
            if (bleConnected) {
                char buf[64];
                snprintf(buf, sizeof(buf),
                    "{\"t\":%.0f,\"p\":%d,\"led\":%d}",
                    weather.data().temp, currentPage, led.isEnabled());
                ble.sendStatus(String(buf));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── Touch & Gesture Handler ──────────────────────────────────
void handleGesture(Gesture g) {
    switch (g) {
        case Gesture::SWIPE_LEFT:
            currentPage = (currentPage + 1) % NUM_PAGES;
            pageChanged = true;
            break;

        case Gesture::SWIPE_RIGHT:
            currentPage = (currentPage + NUM_PAGES - 1) % NUM_PAGES;
            pageChanged = true;
            break;

        case Gesture::TAP: {
            int16_t tx = touch.tapX();
            int16_t ty = touch.tapY();

            // 1. Universal Bottom Navigation Bar (y >= 278) — works on EVERY page
            if (ty >= 278) {
                if (tx <= 100) {
                    currentPage = (currentPage + NUM_PAGES - 1) % NUM_PAGES;
                    pageChanged = true;
                    return;
                } else if (tx >= 140) {
                    currentPage = (currentPage + 1) % NUM_PAGES;
                    pageChanged = true;
                    return;
                }
            }

            // 2. Page-specific touch elements (y < 278)
            if (currentPage == PAGE_CLOCK) {
                // Tapping clock area (y: 68..150) toggles Full-Screen Wallpaper mode
                if (ty >= 68 && ty <= 150) {
                    wallpaper.toggleClockOverlay();
                    clockPage.markDirty();
                    return;
                }
                // Quick control buttons on Clock Page (y: 216..264)
                if (ty >= 216 && ty <= 264) {
                    if (tx <= 116) {
                        // LED Toggle
                        led.setEnabled(!led.isEnabled());
                        clockPage.markDirty();
                        prefs.putBool("led_on", led.isEnabled());
                    } else if (tx >= 124) {
                        // Wallpaper Mode Cycle: Matrix -> Picture -> GIF
                        wallpaper.cycleMode();
                        clockPage.markDirty();
                    }
                }
            } else if (currentPage == PAGE_WEATHER) {
                if (ty >= 230 && ty <= 270) {
                    if (tx < SCREEN_W / 2) {
                        weather.fetch();
                        weatherPage.markDirty();
                    } else {
                        weather.setCelsius(!weather.isCelsius());
                        prefs.putBool("celsius", weather.isCelsius());
                        weatherPage.markDirty();
                    }
                }
            } else if (currentPage == PAGE_RGB) {
                if (rgbPage.handleTap(tx, ty, led)) {
                    prefs.putBool("led_on",  led.isEnabled());
                    prefs.putUChar("led_r",  led.getR());
                    prefs.putUChar("led_g",  led.getG());
                    prefs.putUChar("led_b",  led.getB());
                    prefs.putUChar("led_bri",led.getBrightness());
                    prefs.putUChar("led_mode",led.getMode());
                }
            } else if (currentPage == PAGE_WALLPAPER) {
                // Control row at y: 228..268
                if (ty >= 228 && ty <= 268) {
                    if (tx <= 76) {
                        wallpaper.prevFile();
                        wallpaperPage.markDirty();
                    } else if (tx >= 80 && tx <= 162) {
                        wallpaper.cycleMode();
                        wallpaperPage.markDirty();
                    } else if (tx >= 166) {
                        wallpaper.nextFile();
                        wallpaperPage.markDirty();
                    }
                } else if (ty < 228) {
                    // Tap anywhere on image to view next wallpaper
                    wallpaper.nextFile();
                    wallpaperPage.markDirty();
                }
            } else if (currentPage == PAGE_STATS) {
                if (ty >= 224 && ty <= 268 && tx >= 16 && tx <= 224) {
                    runTouchCalibration();
                }
            }
            break;
        }

        case Gesture::SWIPE_UP:
            setBacklight(min(255, blBrightness + 32));
            prefs.putUChar("bl_brightness", blBrightness);
            break;

        case Gesture::SWIPE_DOWN:
            setBacklight(max(10, blBrightness - 32));
            prefs.putUChar("bl_brightness", blBrightness);
            break;

        default: break;
    }
}

// ── Page transition animation ────────────────────────────────
void pageTransition(uint8_t from, uint8_t to) {
    bool goRight = (to > from) || (from == NUM_PAGES - 1 && to == 0);
    int16_t step = goRight ? 20 : -20;
    for (int16_t x = 0; abs(x) < SCREEN_W; x += step) {
        int16_t lx = goRight ? x : SCREEN_W + x;
        if (lx >= 0 && lx < SCREEN_W)
            tft.drawFastVLine(lx, 0, SCREEN_H, C_CYAN);
        delay(6);
    }
    tft.fillScreen(C_BG);
}

// ── setup() ──────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n[BOOT] ESP32S3-CyberDesk v1.0");

    setCompileTimeFallback();

    // ── Backlight PWM ──
    ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
    ledcAttachPin(PIN_TFT_BL, BL_PWM_CHANNEL);
    ledcWrite(BL_PWM_CHANNEL, 0);

    // ── Preferences ──
    prefs.begin("desk", false);
    blBrightness  = prefs.getUChar("bl_brightness", BL_DEFAULT_BRIGHTNESS);
    bool ledOn    = prefs.getBool("led_on",  true);
    uint8_t ledR  = prefs.getUChar("led_r",  0);
    uint8_t ledG  = prefs.getUChar("led_g",  200);
    uint8_t ledB  = prefs.getUChar("led_b",  255);
    uint8_t ledBr = prefs.getUChar("led_bri",LED_DEFAULT_BRIGHTNESS);
    uint8_t ledMd = prefs.getUChar("led_mode",LedCtrl::MODE_SOLID);
    bool celsius  = prefs.getBool("celsius", true);

    // ── NeoPixel ──
    led.begin();
    led.setBrightness(ledBr);
    led.setColor(ledR, ledG, ledB);
    led.setMode(ledMd);
    led.setEnabled(ledOn);

    // ── SPI & Hardware Reset for ST7789 ──
    pinMode(PIN_TOUCH_CS, OUTPUT);
    digitalWrite(PIN_TOUCH_CS, HIGH);
    pinMode(PIN_TFT_CS, OUTPUT);
    digitalWrite(PIN_TFT_CS, HIGH);
    pinMode(PIN_TFT_DC, OUTPUT);
    digitalWrite(PIN_TFT_DC, HIGH);
    pinMode(PIN_TFT_RST, OUTPUT);
    digitalWrite(PIN_TFT_RST, HIGH);
    delay(10);
    digitalWrite(PIN_TFT_RST, LOW);
    delay(50);
    digitalWrite(PIN_TFT_RST, HIGH);
    delay(150);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(C_BG);

    // Backlight on
    ledcWrite(BL_PWM_CHANNEL, blBrightness);

    // ── Touch Controller Init ──
    uint16_t calData[5];
    if (prefs.getBytes("touchCal", calData, sizeof(calData)) == sizeof(calData)) {
        Serial.printf("[TOUCH] Loaded saved calData: { %d, %d, %d, %d, %d }\n",
            calData[0], calData[1], calData[2], calData[3], calData[4]);
        touch.begin(&tft, calData);
    } else {
        uint16_t defCal[5] = { 331, 3397, 248, 3453, 2 };
        Serial.println("[TOUCH] Using default 2.4\" calibration matrix");
        touch.begin(&tft, defCal);
    }

    // ── Wallpaper Manager Init (LittleFS, Decoders) ──
    wallpaper.begin(&tft);

    // ── Boot splash ──
    drawWifiSplash("STARTING CYBERDESK...", C_CYAN);
    tft.setTextColor(C_GREY, C_BG);
    tft.setTextSize(1);
    tft.setCursor(22, 230);
    tft.print("[ HOLD TOUCH TO CALIBRATE ]");

    uint32_t splashStart = millis();
    bool forceCal = false;
    while (millis() - splashStart < 1200) {
        if (tft.getTouchRawZ() > 300) {
            forceCal = true;
            break;
        }
        delay(50);
    }

    if (forceCal) {
        runTouchCalibration();
    }

    // ── Weather unit ──
    weather.begin(OWM_API_KEY, OWM_CITY, OWM_COUNTRY_CODE);
    weather.setCelsius(celsius);

    // ── Init pages ──
    clockPage.init(tft);
    weatherPage.init(tft);
    rgbPage.init(tft);
    wallpaperPage.init(tft);
    statsPage.init(tft);

    // ── Network task on Core 1 ──
    xTaskCreatePinnedToCore(
        networkTask, "network", 14336, nullptr, 1, nullptr, 1
    );

    Serial.println("[BOOT] ESP32S3-CyberDesk Online");
}

// ── loop() — runs on Core 0 ───────────────────────────────────
void loop() {
    // ── LED animation ──
    led.update();

    // ── Touch input ──
    Gesture g = touch.update();
    if (g != Gesture::NONE) {
        uint8_t prevPage = currentPage;
        handleGesture(g);
        if (currentPage != prevPage) {
            pageTransition(prevPage, currentPage);
            pageChanged = true;
        }
    }

    // ── On page change: mark dirty ──
    if (pageChanged) {
        pageChanged = false;
        switch (currentPage) {
            case PAGE_CLOCK:     clockPage.markDirty();     break;
            case PAGE_WEATHER:   weatherPage.markDirty();   break;
            case PAGE_RGB:       rgbPage.markDirty();       break;
            case PAGE_WALLPAPER: wallpaperPage.markDirty(); break;
            default: break;
        }
    }

    // ── Render current page ──
    switch (currentPage) {
        case PAGE_CLOCK:
            clockPage.draw(ntpClient, led, wallpaper);
            break;
        case PAGE_WEATHER:
            weatherPage.draw(weather);
            break;
        case PAGE_RGB:
            rgbPage.draw(led);
            break;
        case PAGE_WALLPAPER:
            wallpaperPage.draw(wallpaper);
            break;
        case PAGE_STATS:
            statsPage.draw(led, bleConnected, ipAddr);
            break;
    }

    // ── Target ~30 fps ──
    static uint32_t frameTimer = 0;
    int32_t sleep = (1000 / ANIM_FPS) - (int32_t)(millis() - frameTimer);
    if (sleep > 0) delay(sleep);
    frameTimer = millis();
}
