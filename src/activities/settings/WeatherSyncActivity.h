#pragma once

#include "activities/Activity.h"
#include "activities/ScreenTransitionRefresh.h"

// Manual weather refresh action. Connects WiFi when needed, fetches the current
// conditions and a short daily forecast from Open-Meteo, and caches the result
// on the SD card for the Weather sleep screen. The sleep screen itself never
// opens the network, so this activity is the only place weather data is written.
class WeatherSyncActivity final : public Activity {
 public:
  explicit WeatherSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("WeatherSync", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum State { SYNCING, SUCCESS, NO_WIFI, FAILED };
  State state = SYNCING;
  ScreenTransitionRefresh screenTransitionRefresh;
  char summary[72] = {0};
  bool shouldTearDownWifiOnExit = false;

  void runSync();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
};
