---
phase: 2
title: "Weather Model, Cache & Icons"
status: pending
priority: P1
effort: "2h"
dependencies: ["1"]
---

# Phase 02: Weather Model, Cache & Icons

## Overview
Implement the weather data models, SD card cache serialization (`/.crosspoint/weather_cache.json`), WMO code translation, and 1-bit weather icons for clear, partly cloudy, overcast, rain, showers, snow, fog, thunderstorm, and wind.

## Requirements
- Functional:
  - `WeatherData` struct (temperature, feelsLike, humidity, weatherCode, windSpeed).
  - `DailyForecast` struct (weatherCode, tempMax, tempMin, dayLabel).
  - `saveWeatherCache()` and `loadWeatherCache()` reading/writing `/.crosspoint/weather_cache.json`.
  - Icon rasterization / definition for 9 primary weather conditions (32x32 / 48x48) compatible with `GfxRenderer::drawIcon()`.
  - Temperature unit conversion (°C / °F) respecting `SETTINGS.temperatureUnit`.
- Non-functional:
  - Cache uses `ArduinoJson` (or compact stream) without leaking file handles.
  - Icons stored in flash as `static const uint8_t[]` arrays; DRAM footprint = 0.

## Related Code Files
- Create: `lib/Weather/WeatherTypes.h`
- Create: `lib/Weather/WeatherCache.h`
- Create: `lib/Weather/WeatherCache.cpp`
- Create: `src/components/icons/weatherIcons.h`
- Create: `src/components/icons/weatherIcons.manifest`

## Success Criteria
- [ ] Weather cache roundtrips accurately to SD JSON format.
- [ ] Icons display cleanly on 1-bit e-ink without distortion.
