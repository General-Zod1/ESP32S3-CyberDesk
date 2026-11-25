// ============================================================
//  pages.h  —  All 5 page renderers:
//              Clock / Weather / RGB / Wallpaper / Stats
//              with cyberpunk/Blade Runner aesthetics &
//              touch targets tuned for 2.4" display
// ============================================================
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <NTPClient.h>
#include <WiFi.h>
#include "config.h"
#include "colors.h"
#include "weather.h"
#include "led_ctrl.h"
#include "wallpaper_mgr.h"

// ── Matrix rain ──────────────────────────────────────────────
struct RainDrop {
    int16_t  col;       // x position
    int16_t  head;      // current head y
    int16_t  len;       // trail length
    uint8_t  speed;     // pixels per tick
    uint8_t  tick;      // counter
    char     chars[20]; // glyphs in trail
};

static const int RAIN_DROP_COUNT = 28;

class MatrixRain {
public:
    void init(TFT_eSPI& tft) {
        _tft = &tft;
        for (int i = 0; i < RAIN_DROP_COUNT; i++) _resetDrop(i, true);
    }

    // Renders background rain — keeps center HUD area (rows 6..18) clean
    void update() {
        uint32_t now = millis();
        if (now - _lastTick < 70) return;
        _lastTick = now;

        const int maxRow = SCREEN_H / 12; // 26 rows

        for (int i = 0; i < RAIN_DROP_COUNT; i++) {
            RainDrop& d = _drops[i];
            d.tick++;
            if (d.tick < d.speed) continue;
            d.tick = 0;

            // Erase tail pixel (only in rain zones)
            int16_t tail = d.head - d.len;
            if (tail >= 0 && tail < maxRow) {
                if (tail < 6 || tail > 18) {
                    _tft->drawChar(d.col * 8, tail * 12, ' ', C_BG, C_BG, 1);
                }
            }

            // Draw trail
            for (int j = 1; j < d.len; j++) {
                int16_t py = d.head - j;
                if (py < 0 || py >= maxRow) continue;
                if (py >= 6 && py <= 18) continue; // Keep clock HUD clean
                uint16_t c = (j < 3) ? C_GREEN : C_GREEN_DIM;
                _tft->drawChar(d.col * 8, py * 12, d.chars[j % 20], c, C_BG, 1);
            }

            // Draw head (bright white)
            if (d.head >= 0 && d.head < maxRow) {
                if (d.head < 6 || d.head > 18) {
                    char hc = _katakana[random(0, 48)];
                    _tft->drawChar(d.col * 8, d.head * 12, hc, C_WHITE, C_BG, 1);
                    d.chars[d.head % 20] = hc;
                }
            }

            d.head++;
            if (d.head - d.len > maxRow) _resetDrop(i, false);
        }
    }

private:
    TFT_eSPI* _tft = nullptr;
    RainDrop  _drops[RAIN_DROP_COUNT];
    uint32_t  _lastTick = 0;

    static constexpr const char* _katakana = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%&!?|/\\~^";

    void _resetDrop(int i, bool randomY) {
        _drops[i].col   = (i * (SCREEN_W / 8)) / RAIN_DROP_COUNT;
        _drops[i].head  = randomY ? random(-15, 0) : -random(2, 6);
        _drops[i].len   = random(4, 9);
        _drops[i].speed = random(1, 3);
        _drops[i].tick  = 0;
        for (int j = 0; j < 20; j++)
            _drops[i].chars[j] = _katakana[random(0, 48)];
    }
};

// ── Shared drawing helpers ────────────────────────────────────
static void drawCyberpunkBorder(TFT_eSPI& tft, uint16_t color = C_CYAN) {
    tft.drawRect(0, 0, SCREEN_W, SCREEN_H, color);
    tft.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, color);
    // Corner accents
    tft.drawFastHLine(0,  0,  20, C_MAGENTA);
    tft.drawFastVLine(0,  0,  20, C_MAGENTA);
    tft.drawFastHLine(SCREEN_W - 20, 0,  20, C_MAGENTA);
    tft.drawFastVLine(SCREEN_W - 1,  0,  20, C_MAGENTA);
    tft.drawFastHLine(0,  SCREEN_H - 1, 20, C_MAGENTA);
    tft.drawFastVLine(0,  SCREEN_H - 20, 20, C_MAGENTA);
    tft.drawFastHLine(SCREEN_W - 20, SCREEN_H - 1, 20, C_MAGENTA);
    tft.drawFastVLine(SCREEN_W - 1, SCREEN_H - 20, 20, C_MAGENTA);
}

static void drawSectionTitle(TFT_eSPI& tft, int16_t y, const char* txt, uint16_t color = C_CYAN) {
    tft.setTextColor(color);
    tft.setTextSize(1);
    tft.drawFastHLine(5, y, 30, C_MAGENTA);
    tft.setCursor(40, y - 4);
    tft.print(txt);
    tft.drawFastHLine(40 + strlen(txt) * 6 + 4, y, 30, C_MAGENTA);
}

// ── Universal Navigation Bar (y = 278 .. 320) ─────────────────
static void drawPageIndicator(TFT_eSPI& tft, uint8_t current) {
    tft.fillRect(0, 278, SCREEN_W, 42, C_BG);
    tft.drawFastHLine(4, 278, SCREEN_W - 8, C_PANEL);

    // Left button: [ < PREV ] (x: 6..82, y: 283..315)
    tft.fillRoundRect(6, 283, 76, 32, 4, C_PANEL);
    tft.drawRoundRect(6, 283, 76, 32, 4, C_CYAN);
    tft.setTextColor(C_CYAN, C_PANEL);
    tft.setTextSize(1);
    tft.setCursor(18, 295);
    tft.print("< PREV");

    // Right button: [ NEXT > ] (x: 158..234, y: 283..315)
    tft.fillRoundRect(SCREEN_W - 82, 283, 76, 32, 4, C_PANEL);
    tft.drawRoundRect(SCREEN_W - 82, 283, 76, 32, 4, C_MAGENTA);
    tft.setTextColor(C_MAGENTA, C_PANEL);
    tft.setTextSize(1);
    tft.setCursor(SCREEN_W - 70, 295);
    tft.print("NEXT >");

    // Center page dots (5 pages)
    int16_t startX = (SCREEN_W - NUM_PAGES * 14) / 2 + 5;
    for (int i = 0; i < NUM_PAGES; i++) {
        if (i == current) {
            tft.fillCircle(startX + i * 14, 299, 4, C_MAGENTA);
            tft.drawCircle(startX + i * 14, 299, 4, C_CYAN);
        } else {
            tft.drawCircle(startX + i * 14, 299, 3, C_DARK_GREY);
        }
    }
}

// ============================================================
//  PAGE 0 — Clock & HUD (With Wallpaper / GIF Support)
// ============================================================
class ClockPage {
public:
    void init(TFT_eSPI& tft) {
        _tft = &tft;
        _rain.init(tft);
        _dirty = true;
    }

    void markDirty() { _dirty = true; }

    void draw(NTPClient& ntp, const LedCtrl& led, WallpaperMgr& wall) {
        bool     fullRedraw = _dirty;
        uint32_t now = millis();

        // 1. Background rendering
        if (wall.getMode() == WALL_MODE_MATRIX) {
            _rain.update();
        } else {
            wall.renderBackground(fullRedraw);
        }

        // Full redraw on page switch or wallpaper switch
        if (fullRedraw) {
            _dirty = false;
            if (wall.getMode() == WALL_MODE_MATRIX) {
                _tft->fillScreen(C_BG);
                drawCyberpunkBorder(*_tft, C_CYAN);
            }
            if (wall.hasClockOverlay()) {
                _tft->fillRect(8, 8, SCREEN_W - 16, 18, C_PANEL);
                _tft->setTextColor(C_MAGENTA, C_PANEL);
                _tft->setTextSize(1);
                _tft->setCursor(14, 13);
                _tft->print(">> SYSTEM: CYBERDESK v1.0");
                _tft->drawFastHLine(10, 26, SCREEN_W - 20, C_CYAN_DIM);
            }
            drawPageIndicator(*_tft, PAGE_CLOCK);
        }

        // 2. Clock HUD overlay (updates every second)
        if (wall.hasClockOverlay()) {
            if (now - _timeTimer > 1000 || fullRedraw) {
                _timeTimer = now;

                time_t epoch = ntp.getEpochTime();
                if (epoch < 1000000000UL) {
                    epoch = time(NULL);
                }
                struct tm* t = localtime(&epoch);

                // Main time HUD card (semi-transparent panel if wallpaper, pure BG if matrix)
                uint16_t cardBg = (wall.getMode() == WALL_MODE_MATRIX) ? C_BG : C_PANEL;
                _tft->fillRoundRect(10, 68, SCREEN_W - 20, 82, 6, cardBg);
                _tft->drawRoundRect(10, 68, SCREEN_W - 20, 82, 6, C_CYAN);

                // Main time (HH:MM)
                _tft->setTextColor(C_CYAN, cardBg);
                _tft->setTextSize(5);
                _tft->setCursor(18, 76);
                _tft->printf("%02d:%02d", t->tm_hour, t->tm_min);

                // Seconds
                _tft->setTextColor(C_MAGENTA, cardBg);
                _tft->setTextSize(2);
                _tft->setCursor(186, 108);
                _tft->printf(":%02d", t->tm_sec);

                // Date line
                _tft->fillRect(8, 154, SCREEN_W - 16, 18, cardBg);
                const char* days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
                const char* months[] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                        "JUL","AUG","SEP","OCT","NOV","DEC"};
                _tft->setTextColor(C_YELLOW, cardBg);
                _tft->setTextSize(1);
                _tft->setCursor(14, 158);
                _tft->printf("[ %s %02d %s %04d ]", days[t->tm_wday],
                             t->tm_mday, months[t->tm_mon], t->tm_year + 1900);

                // Network & status line
                _tft->fillRect(8, 176, SCREEN_W - 16, 16, cardBg);
                _tft->setCursor(14, 180);
                if (WiFi.status() == WL_CONNECTED) {
                    _tft->setTextColor(C_GREEN, cardBg);
                    _tft->print(">> NET: ONLINE | NTP SYNC");
                } else {
                    _tft->setTextColor(C_RED, cardBg);
                    _tft->print(">> NET: STANDBY | AP MODE");
                }

                // Dual Quick Buttons at bottom of HUD:
                // Button 1 (x: 10..116): RGB LED toggle
                bool ledOn = led.isEnabled();
                _tft->fillRoundRect(10, 218, 106, 44, 4, C_PANEL);
                _tft->drawRoundRect(10, 218, 106, 44, 4, ledOn ? C_CYAN : C_DARK_GREY);
                _tft->fillCircle(24, 234, 4, ledOn ? C_GREEN : C_RED);
                _tft->setTextColor(ledOn ? C_CYAN : C_GREY, C_PANEL);
                _tft->setTextSize(1);
                _tft->setCursor(34, 230);
                _tft->printf("LED: %s", ledOn ? "ON" : "OFF");
                _tft->setCursor(18, 246);
                _tft->setTextColor(C_YELLOW, C_PANEL);
                _tft->print("[TAP TOGGLE]");

                // Button 2 (x: 124..230): Wallpaper Background Mode toggle
                _tft->fillRoundRect(124, 218, 106, 44, 4, C_PANEL);
                _tft->drawRoundRect(124, 218, 106, 44, 4, C_MAGENTA);
                _tft->setTextColor(C_MAGENTA, C_PANEL);
                _tft->setTextSize(1);
                _tft->setCursor(132, 230);
                _tft->print("BG: ");
                _tft->setTextColor(C_WHITE, C_PANEL);
                _tft->print(wall.getMode() == WALL_MODE_MATRIX ? "MATRIX" :
                            wall.getMode() == WALL_MODE_IMAGE  ? "IMAGE" : "GIF");
                _tft->setCursor(132, 246);
                _tft->setTextColor(C_YELLOW, C_PANEL);
                _tft->print("[TAP SWITCH]");

                drawPageIndicator(*_tft, PAGE_CLOCK);
            }
        }
    }

private:
    TFT_eSPI*   _tft = nullptr;
    MatrixRain  _rain;
    bool        _dirty    = true;
    uint32_t    _timeTimer = 0;
};

// ============================================================
//  PAGE 1 — Weather
// ============================================================
class WeatherPage {
public:
    void init(TFT_eSPI& tft) {
        _tft = &tft;
        _dirty = true;
    }

    void markDirty() { _dirty = true; }

    void draw(const Weather& wx) {
        if (!_dirty) return;
        _dirty = false;

        const WeatherData& d = wx.data();
        _tft->fillScreen(C_BG);

        // Grid
        for (int y = 20; y < 278; y += 16)
            _tft->drawFastHLine(0, y, SCREEN_W, C_PANEL);
        for (int x = 20; x < SCREEN_W; x += 20)
            _tft->drawFastVLine(x, 0, 278, C_PANEL);

        drawCyberpunkBorder(*_tft, C_BR_AMBER);

        // Header
        _tft->fillRect(0, 0, SCREEN_W, 28, C_PANEL);
        _tft->drawFastHLine(0, 28, SCREEN_W, C_BR_AMBER);
        _tft->setTextColor(C_BR_AMBER);
        _tft->setTextSize(1);
        _tft->setCursor(6, 6);
        _tft->print("// WEATHER INTEL //");
        _tft->setTextColor(C_GREY);
        _tft->setCursor(6, 16);
        _tft->printf("LOC: %s", d.valid ? d.city : "---");

        if (!d.valid) {
            _tft->setTextColor(C_RED);
            _tft->setTextSize(1);
            _tft->setCursor(30, 110);
            _tft->print("CONNECTING TO WIFI...");
            _tft->setCursor(20, 130);
            _tft->setTextColor(C_GREY);
            _tft->print("Weather syncs automatically");
            
            _tft->fillRoundRect(30, 170, SCREEN_W - 60, 36, 4, C_PANEL);
            _tft->drawRoundRect(30, 170, SCREEN_W - 60, 36, 4, C_BR_AMBER);
            _tft->setTextColor(C_BR_AMBER, C_PANEL);
            _tft->setCursor(50, 182);
            _tft->print("[ TAP TO RETRY ]");

            drawPageIndicator(*_tft, PAGE_WEATHER);
            return;
        }

        // Big temperature
        _tft->setTextColor(C_CYAN);
        _tft->setTextSize(4);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f%s", d.temp, wx.unitStr());
        int16_t tw = strlen(buf) * 24;
        _tft->setCursor((SCREEN_W - tw) / 2, 34);
        _tft->print(buf);

        // Weather icon
        _tft->setTextColor(C_YELLOW);
        _tft->setTextSize(1);
        _tft->setCursor(6, 38);
        _tft->print(wx.iconEmoji(d.icon));

        // Description
        _tft->setTextColor(C_MAGENTA);
        _tft->setTextSize(1);
        _tft->setCursor(8, 74);
        _tft->print(d.desc);

        // Stats grid
        _tft->drawFastHLine(4, 88, SCREEN_W - 8, C_BR_AMBER);

        struct Stat { const char* label; char val[20]; };
        Stat stats[] = {
            {"FEELS", ""},
            {"HUMID", ""},
            {"WIND",  ""},
            {"PRESS", ""},
            {"VIS",   ""},
            {"RANGE", ""},
        };
        snprintf(stats[0].val, 20, "%.0f%s",  d.feelsLike, wx.unitStr());
        snprintf(stats[1].val, 20, "%d%%",     d.humidity);
        snprintf(stats[2].val, 20, "%.1fm/s %s", d.windSpeed, Weather::windDirStr(d.windDir));
        snprintf(stats[3].val, 20, "%dhPa",    d.pressure);
        snprintf(stats[4].val, 20, "%dkm",     d.visibility / 1000);
        snprintf(stats[5].val, 20, "%.0f-%.0f%s", d.tempMin, d.tempMax, wx.unitStr());

        for (int i = 0; i < 6; i++) {
            int col = i % 2;
            int row = i / 2;
            int16_t x = 6 + col * 118;
            int16_t y = 92 + row * 24;
            _tft->setTextColor(C_GREY);
            _tft->setTextSize(1);
            _tft->setCursor(x, y);
            _tft->print(stats[i].label);
            _tft->setCursor(x, y + 10);
            _tft->setTextColor(C_CYAN);
            _tft->print(stats[i].val);
        }

        // Forecast bar
        _tft->drawFastHLine(4, 168, SCREEN_W - 8, C_BR_AMBER);
        _tft->setTextColor(C_BR_AMBER);
        _tft->setTextSize(1);
        _tft->setCursor(6, 172);
        _tft->print("FORECAST");

        for (int i = 0; i < 5; i++) {
            int16_t x = 4 + i * 46;
            time_t t = d.fcTime[i];
            struct tm* tm_ = localtime(&t);
            char timeBuf[6];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:00", tm_->tm_hour);
            _tft->setTextColor(C_GREY);
            _tft->setTextSize(1);
            _tft->setCursor(x, 182);
            _tft->print(timeBuf);
            _tft->setTextColor(C_CYAN);
            _tft->setCursor(x, 194);
            _tft->printf("%.0f", d.fcTemp[i]);
            _tft->setTextColor(C_YELLOW);
            _tft->setCursor(x, 206);
            _tft->print(wx.iconEmoji(d.fcIcon[i])[2]);
        }

        // Touch buttons: Refresh & Unit toggle (y = 232 .. 268)
        _tft->fillRoundRect(10, 232, 105, 36, 4, C_PANEL);
        _tft->drawRoundRect(10, 232, 105, 36, 4, C_CYAN);
        _tft->setTextColor(C_CYAN, C_PANEL);
        _tft->setTextSize(1);
        _tft->setCursor(22, 245);
        _tft->print("[ REFRESH ]");

        _tft->fillRoundRect(125, 232, 105, 36, 4, C_PANEL);
        _tft->drawRoundRect(125, 232, 105, 36, 4, C_MAGENTA);
        _tft->setTextColor(C_MAGENTA, C_PANEL);
        _tft->setTextSize(1);
        _tft->setCursor(140, 245);
        _tft->printf("[ %s -> %s ]", wx.unitStr(), wx.isCelsius() ? "°F" : "°C");

        drawPageIndicator(*_tft, PAGE_WEATHER);
    }

private:
    TFT_eSPI* _tft  = nullptr;
    bool      _dirty = true;
};

// ============================================================
//  PAGE 2 — RGB Control
// ============================================================
class RGBPage {
public:
    void init(TFT_eSPI& tft) {
        _tft = &tft;
        _dirty = true;
    }

    void markDirty() { _dirty = true; }

    void draw(LedCtrl& led) {
        if (!_dirty) return;
        _dirty = false;

        _tft->fillScreen(C_BG);

        // Background hexagon grid
        for (int y = 0; y < 278; y += 22) {
            for (int x = 0; x < SCREEN_W; x += 26) {
                _tft->drawRect(x + (y / 22 % 2) * 13, y, 12, 20, C_PANEL);
            }
        }

        drawCyberpunkBorder(*_tft, C_MAGENTA);

        // Header
        _tft->fillRect(0, 0, SCREEN_W, 28, C_PANEL);
        _tft->drawFastHLine(0, 28, SCREEN_W, C_MAGENTA);
        _tft->setTextColor(C_MAGENTA);
        _tft->setTextSize(1);
        _tft->setCursor(6, 6);
        _tft->print("// NEOPIXEL RGB CTRL //");

        // ON/OFF toggle (x: 10..112, y: 32..64)
        bool on = led.isEnabled();
        uint16_t btnColor = on ? C_GREEN : C_RED;
        _tft->fillRoundRect(10, 32, 102, 32, 4, btnColor);
        _tft->drawRoundRect(10, 32, 102, 32, 4, C_WHITE);
        _tft->setTextColor(C_WHITE, btnColor);
        _tft->setTextSize(2);
        _tft->setCursor(24, 40);
        _tft->print(on ? "ON " : "OFF");

        // Current color swatch (x: 122..230, y: 32..64)
        uint16_t swatch = rgb565(led.getR(), led.getG(), led.getB());
        _tft->fillRoundRect(122, 32, 108, 32, 4, on ? swatch : C_DARK_GREY);
        _tft->drawRoundRect(122, 32, 108, 32, 4, C_WHITE);
        _tft->setTextColor(C_WHITE, on ? swatch : C_DARK_GREY);
        _tft->setTextSize(1);
        _tft->setCursor(136, 44);
        _tft->print(on ? "ACTIVE COLOR" : "LED OFF");

        // Brightness Controls (y: 72..102)
        _tft->fillRoundRect(10, 72, 42, 30, 4, C_PANEL);
        _tft->drawRoundRect(10, 72, 42, 30, 4, C_CYAN);
        _tft->setTextColor(C_CYAN, C_PANEL);
        _tft->setTextSize(2);
        _tft->setCursor(24, 78);
        _tft->print("-");

        _tft->fillRoundRect(58, 72, 124, 30, 4, C_PANEL);
        _tft->drawRoundRect(58, 72, 124, 30, 4, C_CYAN);
        int16_t barW = map(led.getBrightness(), 0, 255, 0, 120);
        if (barW > 0) {
            _tft->fillRect(60, 74, barW, 26, blend565(C_CYAN, C_PANEL, 120));
        }
        _tft->setTextColor(C_WHITE);
        _tft->setTextSize(1);
        _tft->setCursor(84, 82);
        _tft->printf("BRI: %3d%%", led.getBrightness() * 100 / 255);

        _tft->fillRoundRect(188, 72, 42, 30, 4, C_PANEL);
        _tft->drawRoundRect(188, 72, 42, 30, 4, C_CYAN);
        _tft->setTextColor(C_CYAN, C_PANEL);
        _tft->setTextSize(2);
        _tft->setCursor(201, 78);
        _tft->print("+");

        // Presets Header
        drawSectionTitle(*_tft, 110, "COLOR PRESETS", C_MAGENTA);

        struct Preset { const char* name; uint16_t color; };
        Preset presets[6] = {
            {"CYAN",  rgb565(0, 200, 255)},
            {"PINK",  rgb565(255, 0, 128)},
            {"AMBER", rgb565(255, 140, 0)},
            {"GREEN", rgb565(0, 255, 60)},
            {"PURPLE",rgb565(180, 0, 255)},
            {"WHITE", rgb565(255, 255, 255)},
        };

        // Row 1 (y: 118..146)
        for (int i = 0; i < 3; i++) {
            int16_t bx = 10 + i * 76;
            _tft->fillRoundRect(bx, 118, 68, 28, 4, presets[i].color);
            _tft->drawRoundRect(bx, 118, 68, 28, 4, C_WHITE);
            _tft->setTextColor(presets[i].color == rgb565(255,255,255) ? C_BG : C_WHITE, presets[i].color);
            _tft->setTextSize(1);
            _tft->setCursor(bx + 14, 128);
            _tft->print(presets[i].name);
        }

        // Row 2 (y: 152..180)
        for (int i = 0; i < 3; i++) {
            int16_t bx = 10 + i * 76;
            _tft->fillRoundRect(bx, 152, 68, 28, 4, presets[i + 3].color);
            _tft->drawRoundRect(bx, 152, 68, 28, 4, C_WHITE);
            _tft->setTextColor(presets[i + 3].color == rgb565(255,255,255) ? C_BG : C_WHITE, presets[i + 3].color);
            _tft->setTextSize(1);
            _tft->setCursor(bx + 10, 162);
            _tft->print(presets[i + 3].name);
        }

        // Mode buttons (y: 190..220)
        drawSectionTitle(*_tft, 186, "ANIMATION MODE", C_CYAN);
        const char* modes[] = {"SOLID", "BRTEH", "RNBOW", "PULSE"};
        for (int i = 0; i < 4; i++) {
            bool active = (led.getMode() == i);
            uint16_t bc = active ? C_MAGENTA : C_DARK_GREY;
            int16_t bx = 6 + i * 58;
            _tft->fillRoundRect(bx, 192, 52, 28, 4, bc);
            _tft->drawRoundRect(bx, 192, 52, 28, 4, active ? C_CYAN : C_GREY);
            _tft->setTextColor(active ? C_WHITE : C_GREY, bc);
            _tft->setTextSize(1);
            _tft->setCursor(bx + 8, 202);
            _tft->print(modes[i]);
        }

        drawPageIndicator(*_tft, PAGE_RGB);
    }

    bool handleTap(int16_t tx, int16_t ty, LedCtrl& led) {
        if (tx >= 10 && tx <= 112 && ty >= 32 && ty <= 64) {
            led.setEnabled(!led.isEnabled());
            _dirty = true;
            return true;
        }

        if (tx >= 10 && tx <= 52 && ty >= 72 && ty <= 102) {
            led.setBrightness(max(10, (int)led.getBrightness() - 32));
            _dirty = true;
            return true;
        }

        if (tx >= 58 && tx <= 182 && ty >= 72 && ty <= 102) {
            uint8_t val = map(tx, 58, 182, 10, 255);
            led.setBrightness(constrain(val, 10, 255));
            _dirty = true;
            return true;
        }

        if (tx >= 188 && tx <= 230 && ty >= 72 && ty <= 102) {
            led.setBrightness(min(255, (int)led.getBrightness() + 32));
            _dirty = true;
            return true;
        }

        if (ty >= 118 && ty <= 146) {
            if (tx >= 10 && tx <= 78) {
                led.setColor(0, 200, 255);
                _dirty = true; return true;
            } else if (tx >= 86 && tx <= 154) {
                led.setColor(255, 0, 128);
                _dirty = true; return true;
            } else if (tx >= 162 && tx <= 230) {
                led.setColor(255, 140, 0);
                _dirty = true; return true;
            }
        }

        if (ty >= 152 && ty <= 180) {
            if (tx >= 10 && tx <= 78) {
                led.setColor(0, 255, 60);
                _dirty = true; return true;
            } else if (tx >= 86 && tx <= 154) {
                led.setColor(180, 0, 255);
                _dirty = true; return true;
            } else if (tx >= 162 && tx <= 230) {
                led.setColor(255, 255, 255);
                _dirty = true; return true;
            }
        }

        if (ty >= 190 && ty <= 225) {
            for (int i = 0; i < 4; i++) {
                int16_t bx = 6 + i * 58;
                if (tx >= bx && tx <= bx + 52) {
                    led.setMode(i);
                    _dirty = true;
                    return true;
                }
            }
        }

        return false;
    }

private:
    TFT_eSPI* _tft = nullptr;
    bool      _dirty = true;
};

// ============================================================
//  PAGE 3 — Dedicated Wallpaper & Animated GIF Gallery
// ============================================================
class WallpaperPage {
public:
    void init(TFT_eSPI& tft) {
        _tft = &tft;
        _dirty = true;
    }

    void markDirty() { _dirty = true; }

    void draw(WallpaperMgr& wall) {
        bool full = _dirty;
        if (full) {
            _dirty = false;
            _tft->fillScreen(C_BG);

            // Draw current wallpaper or procedural cyberpunk
            wall.renderBackground(true);

            // Top Header
            _tft->fillRect(0, 0, SCREEN_W, 26, C_PANEL);
            _tft->drawFastHLine(0, 26, SCREEN_W, C_MAGENTA);
            _tft->setTextColor(C_MAGENTA, C_PANEL);
            _tft->setTextSize(1);
            _tft->setCursor(6, 6);
            _tft->print("// WALLPAPER GALLERY //");

            // Filename tag
            _tft->setTextColor(C_CYAN, C_PANEL);
            _tft->setCursor(6, 16);
            String fn = wall.getActiveFile();
            if (fn.length() == 0) {
                _tft->print("BUILT-IN SYNTHWAVE [UPLOAD AT WEB UI]");
            } else {
                _tft->printf("FILE: %s", fn.c_str());
            }

            // Bottom control bar (y: 228..268)
            // [ < PREV ] (x: 8..72)
            _tft->fillRoundRect(8, 230, 66, 36, 4, C_PANEL);
            _tft->drawRoundRect(8, 230, 66, 36, 4, C_CYAN);
            _tft->setTextColor(C_CYAN, C_PANEL);
            _tft->setTextSize(1);
            _tft->setCursor(16, 243);
            _tft->print("< PREV");

            // [ MODE ] (x: 80..160)
            _tft->fillRoundRect(80, 230, 80, 36, 4, C_PANEL);
            _tft->drawRoundRect(80, 230, 80, 36, 4, C_YELLOW);
            _tft->setTextColor(C_YELLOW, C_PANEL);
            _tft->setTextSize(1);
            _tft->setCursor(86, 237);
            _tft->print("MODE:");
            _tft->setCursor(86, 249);
            _tft->setTextColor(C_WHITE, C_PANEL);
            _tft->print(wall.getModeName());

            // [ NEXT > ] (x: 168..232)
            _tft->fillRoundRect(166, 230, 66, 36, 4, C_PANEL);
            _tft->drawRoundRect(166, 230, 66, 36, 4, C_MAGENTA);
            _tft->setTextColor(C_MAGENTA, C_PANEL);
            _tft->setTextSize(1);
            _tft->setCursor(176, 243);
            _tft->print("NEXT >");

            drawPageIndicator(*_tft, PAGE_WALLPAPER);
        }

        // If in GIF mode, update animation frame
        if (wall.getMode() == WALL_MODE_GIF) {
            wall.updateGifFrame();
        }
    }

private:
    TFT_eSPI* _tft = nullptr;
    bool      _dirty = true;
};

// ============================================================
//  PAGE 4 — System Stats & Calibration
// ============================================================
class StatsPage {
public:
    void init(TFT_eSPI& tft) {
        _tft = &tft;
    }

    void draw(const LedCtrl& led, bool bleConnected, const char* ipAddr) {
        uint32_t now = millis();
        if (now - _lastDraw < 1000) return;
        _lastDraw = now;

        _tft->fillScreen(C_BG);

        // Scan-line grid
        for (int y = 0; y < 278; y += 4)
            _tft->drawFastHLine(0, y, SCREEN_W, C_PANEL);

        drawCyberpunkBorder(*_tft, C_GREEN);

        // Header
        _tft->fillRect(0, 0, SCREEN_W, 28, C_PANEL);
        _tft->drawFastHLine(0, 28, SCREEN_W, C_GREEN);
        _tft->setTextColor(C_GREEN);
        _tft->setTextSize(1);
        _tft->setCursor(6, 6);
        _tft->print("// SYSTEM DIAGNOSTICS //");
        _tft->setTextColor(C_GREY);
        _tft->setCursor(6, 16);
        char uptimeBuf[24];
        uint32_t up = millis() / 1000;
        snprintf(uptimeBuf, sizeof(uptimeBuf), "UPTIME: %02luh%02lum%02lus", up/3600, (up%3600)/60, up%60);
        _tft->print(uptimeBuf);

        // Diagnostic items
        struct Item { const char* label; char val[36]; uint16_t color; };
        Item items[] = {
            {"WEB PORTAL", "", C_CYAN},
            {"STORAGE",    "", C_GREEN},
            {"WIFI RSSI",  "", C_GREEN},
            {"IP ADDR",    "", C_YELLOW},
            {"BLE STATUS", "", C_MAGENTA},
            {"LED STATUS", "", C_MAGENTA},
            {"FREE HEAP",  "", C_BR_AMBER},
        };

        snprintf(items[0].val, 36, "http://%s/", ipAddr);
        snprintf(items[1].val, 36, "%lu KB FREE / %lu KB",
                 (uint32_t)((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024),
                 (uint32_t)(LittleFS.totalBytes() / 1024));
        int rssi = WiFi.RSSI();
        snprintf(items[2].val, 36, "%d dBm %s", rssi,
                 rssi > -60 ? "[STRONG]" : rssi > -70 ? "[GOOD]" : "[FAIR]");
        snprintf(items[3].val, 36, "%s", ipAddr);
        snprintf(items[4].val, 36, bleConnected ? "CONNECTED [OK]" : "ADVERTISING");
        const char* modes[] = {"SOLID","BREATHE","RAINBOW","PULSE"};
        snprintf(items[5].val, 36, "%s | %s | %d%%",
                 led.isEnabled() ? "ON" : "OFF",
                 modes[led.getMode()],
                 led.getBrightness() * 100 / 255);
        snprintf(items[6].val, 36, "%lu KB (MIN %lu KB)", ESP.getFreeHeap() / 1024, ESP.getMinFreeHeap() / 1024);

        for (int i = 0; i < 7; i++) {
            int16_t y = 34 + i * 26;
            _tft->setTextColor(C_GREY);
            _tft->setTextSize(1);
            _tft->setCursor(8, y);
            _tft->printf("%-10s", items[i].label);
            _tft->drawRect(8, y + 11, SCREEN_W - 16, 11, C_DARK_GREY);
            _tft->setTextColor(items[i].color);
            _tft->setCursor(12, y + 13);
            _tft->print(items[i].val);
        }

        // Calibrate Touch button (y: 226..264)
        _tft->fillRoundRect(16, 226, SCREEN_W - 32, 38, 5, C_PANEL);
        _tft->drawRoundRect(16, 226, SCREEN_W - 32, 38, 5, C_CYAN);
        _tft->setTextColor(C_CYAN, C_PANEL);
        _tft->setTextSize(1);
        _tft->setCursor(32, 236);
        _tft->print(">> CALIBRATE TOUCH SCREEN <<");
        _tft->setCursor(56, 248);
        _tft->setTextColor(C_MAGENTA, C_PANEL);
        _tft->print("[ TAP TO START ]");

        drawPageIndicator(*_tft, PAGE_STATS);
    }

private:
    TFT_eSPI* _tft = nullptr;
    uint32_t  _lastDraw = 0;
};
