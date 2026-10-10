#include "WeatherSyncActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WeatherCache.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

namespace {

// Fallback location when IP geolocation is unavailable: Hà Nội, Việt Nam.
constexpr const char* kFallbackCity = "Hà Nội";
constexpr const char* kFallbackLat = "21.0278";
constexpr const char* kFallbackLon = "105.8342";

constexpr int kForecastDays = 5;

const char* dayLabelForIsoDate(const char* isoDate) {
  static const char* const DAY_LABELS[7] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
  if (!isoDate || isoDate[0] == '\0') return "";

  int year = 0, month = 0, day = 0;
  if (std::sscanf(isoDate, "%d-%d-%d", &year, &month, &day) != 3) return "";
  if (month < 1 || month > 12 || day < 1 || day > 31) return "";

  std::tm value = {};
  value.tm_year = year - 1900;
  value.tm_mon = month - 1;
  value.tm_mday = day;
  value.tm_hour = 12;  // Avoid DST/rounding edge cases shifting the weekday.
  if (std::mktime(&value) == -1) return "";
  if (value.tm_wday < 0 || value.tm_wday > 6) return "";
  return DAY_LABELS[value.tm_wday];
}

bool parseWeather(const std::string& payload, WeatherData& weather, DailyForecast* forecast, int& forecastCount) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;

  JsonObject current = doc["current"];
  if (current.isNull()) return false;

  weather.temperature = current["temperature_2m"] | 0.0f;
  weather.feelsLike = current["apparent_temperature"] | 0.0f;
  weather.humidity = current["relative_humidity_2m"] | 0;
  weather.weatherCode = current["weather_code"] | 0;
  weather.windSpeed = current["wind_speed_10m"] | 0.0f;

  forecastCount = 0;
  JsonObject daily = doc["daily"];
  if (!daily.isNull()) {
    JsonArray codes = daily["weather_code"];
    JsonArray maxTemps = daily["temperature_2m_max"];
    JsonArray minTemps = daily["temperature_2m_min"];
    JsonArray dates = daily["time"];
    // Entry 0 is today; the sleep screen shows the days ahead.
    for (int i = 1; i < static_cast<int>(codes.size()) && forecastCount < kForecastDays; i++) {
      DailyForecast& entry = forecast[forecastCount];
      entry.weatherCode = codes[i] | 0;
      entry.tempMax = maxTemps[i] | 0.0f;
      entry.tempMin = minTemps[i] | 0.0f;
      std::snprintf(entry.dayLabel, sizeof(entry.dayLabel), "%s", dayLabelForIsoDate(dates[i] | ""));
      forecastCount++;
    }
  }
  return true;
}

}  // namespace

void WeatherSyncActivity::onEnter() {
  Activity::onEnter();
  summary[0] = '\0';
  state = SYNCING;

  if (WiFi.status() == WL_CONNECTED) {
    requestUpdate();
    return;
  }

  shouldTearDownWifiOnExit = true;
  launchWifiSelection();
}

void WeatherSyncActivity::onExit() {
  Activity::onExit();

  // Weather is cached on the SD card, so the radio is not needed afterwards.
  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    WiFi.mode(WIFI_OFF);
  }
}

void WeatherSyncActivity::launchWifiSelection() {
  LOG_INF("WEATHER", "Weather sync requested without WiFi, launching WiFi selection");
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void WeatherSyncActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected) {
    LOG_INF("WEATHER", "WiFi selection cancelled before weather sync");
    finish();
    return;
  }

  state = SYNCING;
  requestUpdate();
}

void WeatherSyncActivity::runSync() {
  if (WiFi.status() != WL_CONNECTED) {
    state = NO_WIFI;
    requestUpdate();
    return;
  }

  char cityName[32] = {0};
  const char* latitude = kFallbackLat;
  const char* longitude = kFallbackLon;
  char detectedLat[16] = {0};
  char detectedLon[16] = {0};

  // IP geolocation keeps the screen useful without a location picker. Failure
  // is not fatal: the fallback city keeps the sync working offline-of-that-API.
  std::string geo;
  if (HttpDownloader::fetchUrl("http://ip-api.com/json/?fields=lat,lon,city", geo)) {
    JsonDocument geoDoc;
    if (!deserializeJson(geoDoc, geo)) {
      const float lat = geoDoc["lat"] | 0.0f;
      const float lon = geoDoc["lon"] | 0.0f;
      const char* city = geoDoc["city"] | "";
      if (lat != 0.0f || lon != 0.0f) {
        std::snprintf(detectedLat, sizeof(detectedLat), "%.4f", static_cast<double>(lat));
        std::snprintf(detectedLon, sizeof(detectedLon), "%.4f", static_cast<double>(lon));
        latitude = detectedLat;
        longitude = detectedLon;
        std::snprintf(cityName, sizeof(cityName), "%s", city);
      }
    }
  }
  if (cityName[0] == '\0') {
    std::snprintf(cityName, sizeof(cityName), "%s", kFallbackCity);
  }

  char url[384];
  std::snprintf(url, sizeof(url),
                "http://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
                "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
                "&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=6&timezone=auto",
                latitude, longitude);

  std::string response;
  if (!HttpDownloader::fetchUrl(url, response)) {
    LOG_ERR("WEATHER", "Weather API request failed");
    state = FAILED;
    requestUpdate();
    return;
  }

  WeatherData weather;
  DailyForecast forecast[kForecastDays];
  int forecastCount = 0;
  if (!parseWeather(response, weather, forecast, forecastCount)) {
    LOG_ERR("WEATHER", "Weather response could not be parsed");
    state = FAILED;
    requestUpdate();
    return;
  }

  char updatedAt[16] = {0};
  time_t now = 0;
  time(&now);
  std::tm local = {};
#if defined(_WIN32) && !defined(__CYGWIN__)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  if (local.tm_year >= 125) {
    std::snprintf(updatedAt, sizeof(updatedAt), "%02d:%02d", local.tm_hour, local.tm_min);
  }

  if (!WeatherCache::save(weather, forecast, forecastCount, cityName, updatedAt)) {
    LOG_ERR("WEATHER", "Failed to write the weather cache");
    state = FAILED;
    requestUpdate();
    return;
  }

  std::snprintf(summary, sizeof(summary), "%s %.0f%s", cityName,
                static_cast<double>(WeatherCache::convertTemp(weather.temperature, SETTINGS.temperatureUnit)),
                WeatherCache::tempUnitSuffix(SETTINGS.temperatureUnit));
  LOG_INF("WEATHER", "Weather cached: %s", summary);
  state = SUCCESS;
  requestUpdate();
}

void WeatherSyncActivity::loop() {
  if (state == SYNCING) {
    requestUpdateAndWait();
    runSync();
    return;
  }

  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }

  int x = 0;
  int y = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}

void WeatherSyncActivity::render(RenderLock&&) {
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_SYNC_WEATHER), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_SYNC_WEATHER));
  }

  const int midY = pageHeight / 2;

  switch (state) {
    case SYNCING:
      renderer.drawCenteredText(UI_12_FONT_ID, midY, tr(STR_SYNCING_WEATHER));
      break;
    case SUCCESS:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_CLOCK_SYNC_OK), true, EpdFontFamily::BOLD);
      if (summary[0] != '\0') {
        renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, summary);
      }
      break;
    case NO_WIFI:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_CLOCK_SYNC_NO_WIFI), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_CLOCK_SYNC_NO_WIFI_HINT));
      break;
    case FAILED:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_WEATHER_ERROR), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_CHECK_SERIAL_OUTPUT));
      break;
  }

  if (state != SYNCING) {
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer(screenTransitionRefresh.modeFor(static_cast<uint8_t>(state)));
}
