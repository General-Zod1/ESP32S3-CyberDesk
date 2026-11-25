// ============================================================
//  led_ctrl.h  —  WS2812 NeoPixel controller
// ============================================================
#pragma once
#include <Adafruit_NeoPixel.h>
#include "config.h"

class LedCtrl {
public:
    LedCtrl() : _strip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800) {}

    void begin() {
        _strip.begin();
        _strip.setBrightness(_brightness);
        _strip.clear();
        _strip.show();
    }

    // Solid color
    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        _r = r; _g = g; _b = b;
        _mode = MODE_SOLID;
        _applyColor();
    }

    void setColorHex(uint32_t hex) {
        setColor((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
    }

    void setBrightness(uint8_t br) {
        _brightness = br;
        _strip.setBrightness(br);
        _applyColor();
    }

    uint8_t getBrightness() const { return _brightness; }

    void setEnabled(bool en) {
        _enabled = en;
        if (!en) { _strip.clear(); _strip.show(); }
        else      { _applyColor(); }
    }

    bool isEnabled() const { return _enabled; }

    void setMode(uint8_t mode) { _mode = mode; }

    // Must be called in loop()
    void update() {
        if (!_enabled) return;
        uint32_t now = millis();
        switch (_mode) {
            case MODE_BREATHE:  _breatheUpdate(now); break;
            case MODE_RAINBOW:  _rainbowUpdate(now); break;
            case MODE_PULSE:    _pulseUpdate(now);   break;
            default: break;
        }
    }

    uint8_t getR() const { return _r; }
    uint8_t getG() const { return _g; }
    uint8_t getB() const { return _b; }

    static const uint8_t MODE_SOLID   = 0;
    static const uint8_t MODE_BREATHE = 1;
    static const uint8_t MODE_RAINBOW = 2;
    static const uint8_t MODE_PULSE   = 3;

    uint8_t getMode() const { return _mode; }

private:
    Adafruit_NeoPixel _strip;
    uint8_t _r = 0, _g = 100, _b = 255;    // default: cyber-blue
    uint8_t _brightness = LED_DEFAULT_BRIGHTNESS;
    uint8_t _mode = MODE_SOLID;
    bool    _enabled = true;

    // Animation state
    uint32_t _lastUpdate = 0;
    float    _breathePhase = 0.0f;
    uint16_t _rainbowHue   = 0;

    void _applyColor() {
        if (!_enabled) return;
        _strip.setPixelColor(0, _strip.Color(_r, _g, _b));
        _strip.show();
    }

    void _breatheUpdate(uint32_t now) {
        if (now - _lastUpdate < 20) return;
        _lastUpdate = now;
        _breathePhase += 0.05f;
        if (_breathePhase > TWO_PI) _breathePhase -= TWO_PI;
        float factor = (sin(_breathePhase) + 1.0f) / 2.0f;
        _strip.setBrightness((uint8_t)(_brightness * factor));
        _strip.setPixelColor(0, _strip.Color(_r, _g, _b));
        _strip.show();
    }

    void _rainbowUpdate(uint32_t now) {
        if (now - _lastUpdate < 30) return;
        _lastUpdate = now;
        _rainbowHue += 256;
        uint32_t c = _strip.gamma32(_strip.ColorHSV(_rainbowHue));
        _strip.setBrightness(_brightness);
        _strip.setPixelColor(0, c);
        _strip.show();
    }

    void _pulseUpdate(uint32_t now) {
        if (now - _lastUpdate < 15) return;
        _lastUpdate = now;
        _breathePhase += 0.12f;
        if (_breathePhase > TWO_PI) _breathePhase -= TWO_PI;
        float factor = pow((sin(_breathePhase) + 1.0f) / 2.0f, 3.0f);
        _strip.setBrightness((uint8_t)(_brightness * factor));
        _strip.setPixelColor(0, _strip.Color(_r, _g, _b));
        _strip.show();
    }
};
