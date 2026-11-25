# ESP32S3-CyberDesk

A cyberpunk-themed smart desk companion firmware for **ESP32-S3** featuring a **2.4" ST7789V SPI TFT (240x320)** display, **XPT2046** resistive touch controller, **WS2812** NeoPixel RGB LED, **OpenWeatherMap** forecasts, **LittleFS JPEG & Animated GIF** wallpaper portal, Wi-Fi captive portal, and **BLE** JSON remote control.

---

## Hardware Wiring

| Signal | ESP32-S3 GPIO |
| :--- | :--- |
| TFT CS | 10 |
| TFT RST | 5 |
| TFT DC | 7 |
| MOSI | 11 (FSPI / shared with touch) |
| MISO | 13 (FSPI / shared with touch) |
| SCK | 12 (FSPI / shared with touch) |
| TFT BL | 38 (PWM) |
| Touch CS | 3 |
| WS2812 LED | 48 |

Configured for `esp32-s3-devkitc-1` (8 MB QIO flash) in [platformio.ini](platformio.ini).

---

## Pages (Swipe Left / Right or Bottom Nav Bar)

| # | Page | Description |
| :---: | :--- | :--- |
| 0 | **Clock** | Cyberpunk Matrix rain (or custom JPEG/GIF backdrop), NTP time/date HUD, and quick LED/wallpaper toggles |
| 1 | **Weather** | OpenWeatherMap current temperature, conditions, wind/humidity, and 5-slot (3h) forecast |
| 2 | **RGB Ctrl** | Touch-interactive WS2812 LED color presets, modes (`Solid`, `Breathe`, `Rainbow`, `Pulse`), and brightness |
| 3 | **Wallpaper** | LittleFS JPEG & Animated GIF gallery viewer (falls back to procedural synthwave grid if empty) |
| 4 | **Stats** | Web portal URL, LittleFS free storage, Wi-Fi RSSI, IP address, BLE/LED state, free heap, and 4-point touch calibration |

---

## Touch Gestures

| Gesture | Action |
| :--- | :--- |
| **Swipe Left / Right** | Switch between pages |
| **Swipe Up / Down** | Increase / decrease screen backlight brightness |
| **Tap Bottom Bar** | Previous / Next page buttons |
| **Tap Clock HUD** | Toggle clock overlay over wallpaper |
| **Tap Wallpaper** | Advance to next uploaded JPEG/GIF |
| **Hold Touch at Boot** | Launch interactive 4-corner XPT2046 calibration (saved to flash NVS) |

---

## Web Wallpaper & GIF Portal

Once connected to Wi-Fi (or AP fallback `192.168.4.1`), open **`http://<device-ip>/`** in any browser on the same network to:

- Upload `.jpg` / `.jpeg` images and animated `.gif` files directly to LittleFS
- Switch between `Matrix Rain`, `Custom Pic`, and `Animated GIF` modes
- Select or delete uploaded wallpapers and toggle the Clock HUD overlay

---

## BLE Control (JSON Commands)

Connect to **`ESP32S3-CyberDesk`** via BLE and write JSON payloads to the command characteristic (`12345678-1234-1234-1234-123456789abd`):

```json
{"cmd":"page","val":1}
{"cmd":"led_on","val":true}
{"cmd":"led_color","r":0,"g":200,"b":255}
{"cmd":"led_brightness","val":150}
{"cmd":"led_mode","val":2}
{"cmd":"wallpaper_mode","val":1}
{"cmd":"display_brightness","val":200}
{"cmd":"unit","val":"C"}
{"cmd":"weather_refresh"}
{"cmd":"status"}
```

---

## First-Time Configuration

1. Edit [include/config.h](include/config.h) before flashing:
   - Set `OWM_API_KEY`, `OWM_CITY`, and `OWM_COUNTRY_CODE` (and `NTP_UTC_OFFSET` for your timezone)
   - Optionally set `WIFI_SSID_DEFAULT` / `WIFI_PASS_DEFAULT` (if left as defaults, the device starts the **WiFiManager** captive portal directly)
2. Build and flash the firmware with PlatformIO.
3. On first boot (without hardcoded or saved Wi-Fi), connect to hotspot **`CyberDesk-Setup`** (password: `setup1234`) to configure Wi-Fi from your phone or PC.

---

## Building & Flashing

Requires [PlatformIO](https://platformio.org/):

```bash
pio run
pio run --target upload
pio device monitor
```

---

## License

MIT License — see [LICENSE](LICENSE).
