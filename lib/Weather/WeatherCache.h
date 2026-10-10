#pragma once

#include <cstddef>
#include <cstdint>

#include "WeatherTypes.h"

class WeatherCache {
 public:
  static constexpr int MAX_FORECAST_DAYS = 5;
  static constexpr const char* CACHE_PATH = "/.crosspoint/weather_cache.json";

  // Load weather cache from /.crosspoint/weather_cache.json
  // Returns true if valid cache was found and parsed, false otherwise
  static bool load(WeatherData& outWeather, DailyForecast* outForecast, int maxForecast, int& outForecastCount,
                   char* cityNameBuf, size_t cityBufLen, char* updateTimeBuf, size_t timeBufLen);

  // Save weather data to /.crosspoint/weather_cache.json
  static bool save(const WeatherData& weather, const DailyForecast* forecast, int forecastCount, const char* cityName,
                   const char* updateTime);

  // Temperature unit helpers: unit 0 = °C, unit 1 = °F
  static float convertTemp(float celsius, uint8_t unit) {
    if (unit == 1) return celsius * 9.0f / 5.0f + 32.0f;
    return celsius;
  }

  static const char* tempUnitSuffix(uint8_t unit) { return unit == 1 ? "°F" : "°C"; }

  // WMO weather code string (English fallback)
  static const char* weatherCodeToEnglish(int code);
};
