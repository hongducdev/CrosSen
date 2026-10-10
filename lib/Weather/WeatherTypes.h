#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Weather data structures and helpers.
 */

struct WeatherData {
  float temperature = 0.0f;
  float feelsLike = 0.0f;
  int humidity = 0;
  int weatherCode = 0;
  float windSpeed = 0.0f;
};

struct DailyForecast {
  int weatherCode = 0;
  float tempMax = 0.0f;
  float tempMin = 0.0f;
  char dayLabel[8] = "";  // "Mon", "Tue", "T2", "T3", etc.
};
