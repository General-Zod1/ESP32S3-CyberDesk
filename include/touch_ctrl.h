// ============================================================
//  touch_ctrl.h  —  TFT_eSPI native XPT2046 touch + gestures
//                   Calibrated, debounced & tuned for 2.4"
// ============================================================
#pragma once
#include <TFT_eSPI.h>
#include "config.h"

enum class Gesture { NONE, TAP, SWIPE_LEFT, SWIPE_RIGHT, SWIPE_UP, SWIPE_DOWN };

struct TouchPoint {
    int16_t x = 0, y = 0;
    bool    pressed = false;
};

class TouchCtrl {
public:
    void begin(TFT_eSPI* tft, const uint16_t* calData = nullptr) {
        _tft = tft;
        pinMode(PIN_TOUCH_CS, OUTPUT);
        digitalWrite(PIN_TOUCH_CS, HIGH);

        if (calData) {
            _tft->setTouch((uint16_t*)calData);
        } else {
            // Default 2.4" 240x320 portrait (rotation 0) calibration
            // parameters[4] = 2 means invert_x = true, rotate = false, invert_y = false
            uint16_t defCal[5] = { 331, 3397, 248, 3453, 2 };
            _tft->setTouch(defCal);
        }
    }

    void setCalibration(const uint16_t* calData) {
        if (_tft && calData) {
            _tft->setTouch((uint16_t*)calData);
        }
    }

    TouchPoint read() {
        TouchPoint tp;
        if (!_tft) return tp;

        uint16_t tx = 0, ty = 0;
        // TFT_eSPI::getTouch reads pressure, averages samples, checks deadband,
        // and converts to screen pixels (0..239, 0..319) using calibration matrix
        bool touched = _tft->getTouch(&tx, &ty, 350);

        if (touched) {
            tp.x = constrain((int16_t)tx, (int16_t)0, (int16_t)(SCREEN_W - 1));
            tp.y = constrain((int16_t)ty, (int16_t)0, (int16_t)(SCREEN_H - 1));
            tp.pressed = true;
            _lastTouchTime = millis();
            _lastX = tp.x;
            _lastY = tp.y;

            static uint32_t lastDbg = 0;
            if (millis() - lastDbg > 300) {
                lastDbg = millis();
                Serial.printf("[TOUCH] pixel=(%d, %d)\n", tp.x, tp.y);
            }
        } else {
            // Resistive contact debounce: bridge brief pressure drops (< 80ms)
            if (_wasTouched && (millis() - _lastTouchTime < 80)) {
                tp.x = _lastX;
                tp.y = _lastY;
                tp.pressed = true;
            }
        }
        return tp;
    }

    Gesture update() {
        TouchPoint tp = read();
        uint32_t now = millis();

        if (tp.pressed) {
            if (!_wasTouched) {
                // Touch down
                _startX     = tp.x;
                _startY     = tp.y;
                _touchStart = now;
                _wasTouched = true;
            }
        } else if (_wasTouched) {
            // Touch release
            _wasTouched = false;
            uint32_t dur = now - _touchStart;
            int16_t dx = _lastX - _startX;
            int16_t dy = _lastY - _startY;

            _tapX = _startX;
            _tapY = _startY;

            // Horizontal Swipe: displacement >= 35px, predominately horizontal
            if (abs(dx) >= 35 && abs(dx) > abs(dy) * 1.2) {
                Serial.printf("[GESTURE] SWIPE %s (dx=%d, dy=%d)\n", dx > 0 ? "RIGHT" : "LEFT", dx, dy);
                return dx > 0 ? Gesture::SWIPE_RIGHT : Gesture::SWIPE_LEFT;
            }
            // Vertical Swipe: displacement >= 40px, predominately vertical
            if (abs(dy) >= 40 && abs(dy) > abs(dx) * 1.2) {
                Serial.printf("[GESTURE] SWIPE %s (dy=%d, dx=%d)\n", dy > 0 ? "DOWN" : "UP", dy, dx);
                return dy > 0 ? Gesture::SWIPE_DOWN : Gesture::SWIPE_UP;
            }

            // Tap: short press without significant movement
            if (dur < 750 && abs(dx) < 28 && abs(dy) < 28) {
                Serial.printf("[GESTURE] TAP at (%d, %d)\n", _tapX, _tapY);
                return Gesture::TAP;
            }
        }

        return Gesture::NONE;
    }

    int16_t tapX() const { return _tapX; }
    int16_t tapY() const { return _tapY; }
    int16_t currentX() const { return _lastX; }
    int16_t currentY() const { return _lastY; }
    bool    isTouched() const { return _wasTouched; }

private:
    TFT_eSPI*   _tft = nullptr;
    bool        _wasTouched = false;
    uint32_t    _lastTouchTime = 0;
    int16_t     _startX = 0, _startY = 0;
    int16_t     _lastX  = 0, _lastY  = 0;
    int16_t     _tapX   = 0, _tapY   = 0;
    uint32_t    _touchStart = 0;
};
