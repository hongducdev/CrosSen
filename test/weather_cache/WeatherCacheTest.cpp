#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../lib/Weather/WeatherCache.h"
#include "../../lib/Weather/WeatherTypes.h"
#include "../persistable_store_atomic/stubs/HalStorage.h"

int main() {
  (void)sizeof(HalStorage);
  printf("Starting WeatherCache tests...\n");

  // 1. Temperature conversions
  {
    assert(std::fabs(WeatherCache::convertTemp(0.0f, 0) - 0.0f) < 0.01f);
    assert(std::fabs(WeatherCache::convertTemp(0.0f, 1) - 32.0f) < 0.01f);
    assert(std::fabs(WeatherCache::convertTemp(100.0f, 1) - 212.0f) < 0.01f);
    assert(std::fabs(WeatherCache::convertTemp(25.0f, 1) - 77.0f) < 0.01f);

    assert(std::strcmp(WeatherCache::tempUnitSuffix(0), "°C") == 0);
    assert(std::strcmp(WeatherCache::tempUnitSuffix(1), "°F") == 0);
  }

  // 2. Weather code strings
  {
    assert(std::strcmp(WeatherCache::weatherCodeToEnglish(0), "Clear sky") == 0);
    assert(std::strcmp(WeatherCache::weatherCodeToEnglish(2), "Partly cloudy") == 0);
    assert(std::strcmp(WeatherCache::weatherCodeToEnglish(45), "Fog") == 0);
    assert(std::strcmp(WeatherCache::weatherCodeToEnglish(53), "Drizzle") == 0);
    assert(std::strcmp(WeatherCache::weatherCodeToEnglish(63), "Rain") == 0);
    assert(std::strcmp(WeatherCache::weatherCodeToEnglish(73), "Snow") == 0);
    assert(std::strcmp(WeatherCache::weatherCodeToEnglish(81), "Showers") == 0);
    assert(std::strcmp(WeatherCache::weatherCodeToEnglish(95), "Thunderstorm") == 0);
  }

  // 3. Data structures
  {
    WeatherData w;
    w.temperature = 28.5f;
    w.feelsLike = 31.0f;
    w.humidity = 78;
    w.weatherCode = 2;
    w.windSpeed = 14.2f;

    assert(w.humidity == 78);
    assert(w.weatherCode == 2);

    DailyForecast f[3];
    f[0].weatherCode = 0;
    f[0].tempMax = 32.0f;
    f[0].tempMin = 25.0f;
    std::strncpy(f[0].dayLabel, "T2", sizeof(f[0].dayLabel) - 1);

    assert(std::strcmp(f[0].dayLabel, "T2") == 0);
  }

  // 4. Negative temperatures and conversions
  {
    assert(std::fabs(WeatherCache::convertTemp(-10.0f, 0) - (-10.0f)) < 0.01f);
    assert(std::fabs(WeatherCache::convertTemp(-10.0f, 1) - 14.0f) < 0.01f);
    assert(std::fabs(WeatherCache::convertTemp(-40.0f, 1) - (-40.0f)) < 0.01f);
  }

  // 5. Stale / Empty / Missing cache handling (using Storage mock)
  {
    Storage.reset();
    WeatherData w = {};
    DailyForecast f[5] = {};
    int count = 0;
    char city[32] = "";
    char timeStr[16] = "";

    // 5a. Missing file: must safely return false and leave buffers clean
    assert(!WeatherCache::load(w, f, 5, count, city, sizeof(city), timeStr, sizeof(timeStr)));
    assert(count == 0);

    // 5b. Empty file: must return false
    Storage.writeFile(WeatherCache::CACHE_PATH, "");
    assert(!WeatherCache::load(w, f, 5, count, city, sizeof(city), timeStr, sizeof(timeStr)));
    assert(count == 0);

    // 5c. Corrupt / invalid JSON: must return false without crash
    Storage.writeFile(WeatherCache::CACHE_PATH, "{ this is not json: [ broken ");
    assert(!WeatherCache::load(w, f, 5, count, city, sizeof(city), timeStr, sizeof(timeStr)));
    assert(count == 0);
  }

  // 6. Cache Save and Load Round-trip
  {
    Storage.reset();
    WeatherData wIn;
    wIn.temperature = 22.4f;
    wIn.feelsLike = 24.1f;
    wIn.humidity = 82;
    wIn.weatherCode = 61;  // Rain
    wIn.windSpeed = 15.6f;

    DailyForecast fIn[3];
    fIn[0].weatherCode = 0;
    fIn[0].tempMax = 28.0f;
    fIn[0].tempMin = 20.0f;
    std::strncpy(fIn[0].dayLabel, "T3", sizeof(fIn[0].dayLabel) - 1);

    fIn[1].weatherCode = 2;
    fIn[1].tempMax = 27.5f;
    fIn[1].tempMin = 19.5f;
    std::strncpy(fIn[1].dayLabel, "T4", sizeof(fIn[1].dayLabel) - 1);

    fIn[2].weatherCode = 80;
    fIn[2].tempMax = 25.0f;
    fIn[2].tempMin = 18.0f;
    std::strncpy(fIn[2].dayLabel, "T5", sizeof(fIn[2].dayLabel) - 1);

    assert(WeatherCache::save(wIn, fIn, 3, "Đà Nẵng", "14:20"));

    WeatherData wOut = {};
    DailyForecast fOut[5] = {};
    int countOut = 0;
    char cityOut[32] = "";
    char timeOut[16] = "";

    assert(WeatherCache::load(wOut, fOut, 5, countOut, cityOut, sizeof(cityOut), timeOut, sizeof(timeOut)));
    assert(std::fabs(wOut.temperature - 22.4f) < 0.01f);
    assert(std::fabs(wOut.feelsLike - 24.1f) < 0.01f);
    assert(wOut.humidity == 82);
    assert(wOut.weatherCode == 61);
    assert(std::fabs(wOut.windSpeed - 15.6f) < 0.01f);
    assert(std::strcmp(cityOut, "Đà Nẵng") == 0);
    assert(std::strcmp(timeOut, "14:20") == 0);
    assert(countOut == 3);

    assert(fOut[0].weatherCode == 0);
    assert(std::strcmp(fOut[0].dayLabel, "T3") == 0);
    assert(std::fabs(fOut[0].tempMax - 28.0f) < 0.01f);
    assert(std::fabs(fOut[0].tempMin - 20.0f) < 0.01f);

    assert(fOut[1].weatherCode == 2);
    assert(std::strcmp(fOut[1].dayLabel, "T4") == 0);

    assert(fOut[2].weatherCode == 80);
    assert(std::strcmp(fOut[2].dayLabel, "T5") == 0);
  }

  printf("All WeatherCache tests passed successfully!\n");
  return 0;
}
