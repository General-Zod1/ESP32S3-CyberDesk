// ============================================================
//  weather.h  —  OpenWeatherMap fetcher + data store
// ============================================================
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

struct WeatherData {
    float    temp       = 0.0f;
    float    tempMin    = 0.0f;
    float    tempMax    = 0.0f;
    float    feelsLike  = 0.0f;
    uint8_t  humidity   = 0;
    uint16_t pressure   = 0;
    float    windSpeed  = 0.0f;
    uint16_t windDir    = 0;
    uint16_t visibility = 0;
    char     desc[48]   = "---";
    char     icon[4]    = "01d";
    char     city[32]   = "---";
    uint32_t sunrise    = 0;
    uint32_t sunset     = 0;
    bool     valid      = false;

    // Forecast (5 slots, 3h intervals)
    float    fcTemp[5]  = {};
    char     fcIcon[5][4] = {};
    uint32_t fcTime[5]  = {};
};

class Weather {
public:
    Weather() {}

    void begin(const char* apiKey, const char* city, const char* country) {
        _apiKey  = apiKey;
        _city    = city;
        _country = country;
    }

    bool isCelsius() const { return _celsius; }
    void setCelsius(bool c) { _celsius = c; }

    float convertTemp(float k) const {
        float c = k - 273.15f;
        return _celsius ? c : (c * 9.0f / 5.0f + 32.0f);
    }

    const char* unitStr() const { return _celsius ? "°C" : "°F"; }

    // Call this in a task or periodically — blocks for HTTP
    bool fetch() {
        if (WiFi.status() != WL_CONNECTED) return false;

        // Current weather
        String url = "http://api.openweathermap.org/data/2.5/weather?q=";
        url += _city; url += ","; url += _country;
        url += "&appid="; url += _apiKey;

        HTTPClient http;
        http.begin(url);
        http.setTimeout(8000);
        int code = http.GET();
        if (code != 200) { http.end(); return false; }

        String body = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) return false;

        _data.temp      = convertTemp(doc["main"]["temp"].as<float>());
        _data.tempMin   = convertTemp(doc["main"]["temp_min"].as<float>());
        _data.tempMax   = convertTemp(doc["main"]["temp_max"].as<float>());
        _data.feelsLike = convertTemp(doc["main"]["feels_like"].as<float>());
        _data.humidity  = doc["main"]["humidity"].as<uint8_t>();
        _data.pressure  = doc["main"]["pressure"].as<uint16_t>();
        _data.windSpeed = doc["wind"]["speed"].as<float>();
        _data.windDir   = doc["wind"]["deg"] | 0;
        _data.visibility= doc["visibility"] | 0;
        _data.sunrise   = doc["sys"]["sunrise"].as<uint32_t>();
        _data.sunset    = doc["sys"]["sunset"].as<uint32_t>();
        strlcpy(_data.city, doc["name"].as<const char*>(), sizeof(_data.city));
        strlcpy(_data.desc, doc["weather"][0]["description"].as<const char*>(), sizeof(_data.desc));
        strlcpy(_data.icon, doc["weather"][0]["icon"].as<const char*>(), sizeof(_data.icon));
        // Capitalize description
        if (_data.desc[0] >= 'a' && _data.desc[0] <= 'z') _data.desc[0] -= 32;
        _data.valid = true;

        // Forecast (next 5 slots)
        url = "http://api.openweathermap.org/data/2.5/forecast?q=";
        url += _city; url += ","; url += _country;
        url += "&appid="; url += _apiKey;
        url += "&cnt=5";

        http.begin(url);
        http.setTimeout(8000);
        code = http.GET();
        if (code == 200) {
            body = http.getString();
            JsonDocument fdoc;
            if (deserializeJson(fdoc, body) == DeserializationError::Ok) {
                for (int i = 0; i < 5; i++) {
                    _data.fcTemp[i] = convertTemp(fdoc["list"][i]["main"]["temp"].as<float>());
                    _data.fcTime[i] = fdoc["list"][i]["dt"].as<uint32_t>();
                    strlcpy(_data.fcIcon[i], fdoc["list"][i]["weather"][0]["icon"].as<const char*>(), 4);
                }
            }
        }
        http.end();

        _lastFetch = millis();
        return true;
    }

    bool needsUpdate() const {
        return millis() - _lastFetch > OWM_UPDATE_INTERVAL || _lastFetch == 0;
    }

    const WeatherData& data() const { return _data; }

    // Weather icon to single unicode char (using ASCII art fallback via char)
    // Returns a string suitable for display
    const char* iconEmoji(const char* icon) const {
        String s(icon);
        if (s.startsWith("01")) return "  *  ";  // clear
        if (s.startsWith("02")) return " >=  ";  // few clouds
        if (s.startsWith("03") || s.startsWith("04")) return "  =  "; // clouds
        if (s.startsWith("09")) return "  :  ";  // drizzle
        if (s.startsWith("10")) return " =:= ";  // rain
        if (s.startsWith("11")) return " =!= ";  // thunder
        if (s.startsWith("13")) return " *** ";  // snow
        if (s.startsWith("50")) return " ... ";  // mist
        return "  ?  ";
    }

    // Wind direction to compass string
    static const char* windDirStr(uint16_t deg) {
        const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW","N"};
        return dirs[(deg + 22) / 45 % 8];
    }

private:
    const char* _apiKey  = "";
    const char* _city    = "";
    const char* _country = "";
    WeatherData _data;
    uint32_t    _lastFetch = 0;
    bool        _celsius   = true;
};
