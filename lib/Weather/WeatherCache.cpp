#include "WeatherCache.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

bool WeatherCache::load(WeatherData& outWeather, DailyForecast* outForecast, int maxForecast, int& outForecastCount,
                        char* cityNameBuf, size_t cityBufLen, char* updateTimeBuf, size_t timeBufLen) {
  outForecastCount = 0;
  if (!Storage.exists(CACHE_PATH)) {
    return false;
  }

  String content = Storage.readFile(CACHE_PATH);
  if (content.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, content);
  if (err) {
    LOG_ERR("WEATHER", "Cache deserialize failed: %s", err.c_str());
    return false;
  }

  outWeather.temperature = doc["temp"] | 0.0f;
  outWeather.feelsLike = doc["feels"] | 0.0f;
  outWeather.humidity = doc["hum"] | 0;
  outWeather.weatherCode = doc["code"] | 0;
  outWeather.windSpeed = doc["wind"] | 0.0f;

  if (cityNameBuf && cityBufLen > 0) {
    const char* city = doc["city"] | "";
    std::strncpy(cityNameBuf, city, cityBufLen - 1);
    cityNameBuf[cityBufLen - 1] = '\0';
  }

  if (updateTimeBuf && timeBufLen > 0) {
    const char* t = doc["time"] | "";
    std::strncpy(updateTimeBuf, t, timeBufLen - 1);
    updateTimeBuf[timeBufLen - 1] = '\0';
  }

  if (outForecast && maxForecast > 0 && doc["forecast"].is<JsonArray>()) {
    JsonArray arr = doc["forecast"].as<JsonArray>();
    for (JsonObject obj : arr) {
      if (outForecastCount >= maxForecast) break;
      DailyForecast& f = outForecast[outForecastCount++];
      f.weatherCode = obj["code"] | 0;
      f.tempMax = obj["max"] | 0.0f;
      f.tempMin = obj["min"] | 0.0f;
      const char* day = obj["day"] | "";
      std::strncpy(f.dayLabel, day, sizeof(f.dayLabel) - 1);
      f.dayLabel[sizeof(f.dayLabel) - 1] = '\0';
    }
  }

  return true;
}

bool WeatherCache::save(const WeatherData& weather, const DailyForecast* forecast, int forecastCount,
                        const char* cityName, const char* updateTime) {
  JsonDocument doc;
  doc["temp"] = weather.temperature;
  doc["feels"] = weather.feelsLike;
  doc["hum"] = weather.humidity;
  doc["code"] = weather.weatherCode;
  doc["wind"] = weather.windSpeed;
  doc["city"] = cityName ? cityName : "";
  doc["time"] = updateTime ? updateTime : "";

  if (forecast && forecastCount > 0) {
    JsonArray arr = doc["forecast"].to<JsonArray>();
    for (int i = 0; i < forecastCount; i++) {
      JsonObject obj = arr.add<JsonObject>();
      obj["code"] = forecast[i].weatherCode;
      obj["max"] = forecast[i].tempMax;
      obj["min"] = forecast[i].tempMin;
      obj["day"] = forecast[i].dayLabel;
    }
  }

  String json;
  serializeJson(doc, json);

  Storage.mkdir("/.crosspoint");
  return Storage.writeFile(CACHE_PATH, json);
}

const char* WeatherCache::weatherCodeToEnglish(int code) {
  if (code == 0) return "Clear sky";
  if (code <= 3) return "Partly cloudy";
  if (code <= 48) return "Fog";
  if (code <= 57) return "Drizzle";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 82) return "Showers";
  return "Thunderstorm";
}
