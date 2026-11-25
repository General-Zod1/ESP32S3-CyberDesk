// ============================================================
// config.h  —  ESP32S3-CyberDesk Configuration
// ============================================================
#pragma once

// ---- WiFi (defaults, can be changed via WiFiManager portal) ----
#define WIFI_SSID_DEFAULT     "YOUR_WIFI_SSID"
#define WIFI_PASS_DEFAULT     "YOUR_WIFI_PASSWORD"
#define WIFI_PORTAL_SSID      "CyberDesk-Setup"
#define WIFI_PORTAL_PASS      "setup1234"
#define WIFI_TIMEOUT_SEC      30        // seconds before giving up

// ---- OpenWeatherMap ----
#define OWM_API_KEY           "YOUR_API_KEY_HERE"   // replace with your key
#define OWM_CITY              "Cairo"               // your city
#define OWM_COUNTRY_CODE      "EG"                  // ISO country code
#define OWM_UPDATE_INTERVAL   600000UL              // 10 minutes in ms

// ---- NTP ----
#define NTP_SERVER1           "pool.ntp.org"
#define NTP_SERVER2           "time.nist.gov"
#define NTP_UTC_OFFSET        10800L                // UTC+3 (Cairo)

// ---- Hardware Pins ----
#define PIN_LED               48    // WS2812 data
#define PIN_TFT_CS            10
#define PIN_TFT_RST            5
#define PIN_TFT_DC             7
#define PIN_TFT_MOSI          11
#define PIN_TFT_MISO          13
#define PIN_TFT_SCLK          12
#define PIN_TFT_BL            38
#define PIN_TOUCH_CS           3

// ---- Display ----
#define SCREEN_W              240
#define SCREEN_H              320
#define BL_DEFAULT_BRIGHTNESS 200     // 0–255 PWM
#define BL_PWM_CHANNEL         0
#define BL_PWM_FREQ         5000
#define BL_PWM_RESOLUTION      8

// ---- NeoPixel ----
#define NUM_LEDS               1
#define LED_DEFAULT_BRIGHTNESS 100    // 0–255

// ---- Touch calibration (XPT2046, portrait) ----
// Run calibration sketch to get your exact values
#define TOUCH_CAL_X_MIN       300
#define TOUCH_CAL_X_MAX      3800
#define TOUCH_CAL_Y_MIN       300
#define TOUCH_CAL_Y_MAX      3800
#define TOUCH_ROTATION         1      // match screen rotation

// ---- Pages ----
#define PAGE_CLOCK             0
#define PAGE_WEATHER           1
#define PAGE_RGB               2
#define PAGE_WALLPAPER         3
#define PAGE_STATS             4
#define NUM_PAGES              5

// ---- Animation ----
#define ANIM_FPS              30
#define RAIN_COLS             30      // Matrix rain columns
#define SWIPE_THRESHOLD       40      // px to trigger page change

// ---- BLE ----
#define BLE_DEVICE_NAME       "ESP32S3-CyberDesk"
#define BLE_SERVICE_UUID      "12345678-1234-1234-1234-123456789abc"
#define BLE_CHAR_CMD_UUID     "12345678-1234-1234-1234-123456789abd"
#define BLE_CHAR_STATUS_UUID  "12345678-1234-1234-1234-123456789abe"
