#pragma once

#include "AppCapabilities.h"

#if CROSSINK_APP_CAP_BLE

#include "activities/Activity.h"

// Settings > System > Bluetooth.
//
// Turns the BLE HID central on and off, pairs a new page turner or keyboard,
// reconnects to the saved one, and forgets it. All radio work happens in the
// manager's worker task; this screen only issues requests and polls state.
class BluetoothSettingsActivity final : public Activity {
 public:
  explicit BluetoothSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BluetoothSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // Keep the UI responsive while a scan or connect worker is running.
  bool skipLoopDelay() override;

 private:
  enum Row : uint8_t { ROW_ENABLE = 0, ROW_PAIR, ROW_RECONNECT, ROW_FORGET, ROW_COUNT };

  bool rowVisible(Row row) const;
  uint8_t buildVisibleOrder(uint8_t* out) const;
  void moveSelection(int delta);
  void activateRow(Row row);
  void listLayout(int& top, int& rowStep) const;
  void buildStatusText(char* out, size_t outLen) const;
  void refreshIfStateChanged();

  uint8_t selectedIndex = ROW_ENABLE;
  char lastStatus[96] = "";
};

#endif  // CROSSINK_APP_CAP_BLE
