#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstddef>
#include <cstdint>

#include "DeviceProfiles.h"

class HalGPIO;
class NimBLEClient;
class NimBLERemoteCharacteristic;

// BLE HID central for a Bluetooth keyboard or a Bluetooth page turner.
//
// Threading contract (AGENTS.md: never mutate activity/renderer state from an
// arbitrary task):
//  * The NimBLE host task and the worker task only write into this object's POD
//    queue and volatile flags. They never touch HalGPIO, activities or the
//    renderer.
//  * applyEvents() runs on the main loop only, and is the single place that
//    writes virtual button state into HalGPIO.
class BluetoothHIDManager {
 public:
  enum class State : uint8_t { Disabled, Idle, Scanning, Pairing, Reconnecting, Connected, Error };

  // Result of one main-loop drain.
  struct AppliedInput {
    int8_t pageTurn = 0;  // +1 = next page, -1 = previous page, 0 = none
  };

  static BluetoothHIDManager& getInstance();

  // --- lifecycle -----------------------------------------------------------
  bool enable();
  bool disable();
  bool isEnabled() const { return _enabled; }

  // --- actions: start a bounded worker task, never block the caller --------
  bool startPairNew(uint32_t scanMs = 20000);
  bool startReconnect(uint32_t scanMs = 20000);
  void forgetBond();
  bool isWorkerRunning() const { return _workerRunning; }

  // --- state for the settings screen --------------------------------------
  State state() const { return _state; }
  bool hasBond() const { return _bondedAddress[0] != '\0'; }
  const char* bondedAddress() const { return _bondedAddress; }
  const char* bondedName() const { return _bondedName; }
  uint8_t bondedAddressType() const { return _bondedAddressType; }
  const char* connectedName() const { return _connectedName; }
  const char* lastError() const { return _lastError; }
  bool bondedLooksLikePageTurner() const { return _bondedLooksLikePageTurner; }

  // Restore/clear the bond remembered in settings. No radio work happens here.
  void setBondedDevice(const char* address, const char* name, uint8_t addressType);
  void clearBondedDevice();
  // True once after a successful pairing or a forget, so the caller persists
  // settings exactly once per change.
  bool consumeBondChanged() {
    const bool changed = _bondChanged;
    _bondChanged = false;
    return changed;
  }

  // --- main-loop integration ----------------------------------------------
  // Drains at most one queued HID report and latches the resulting virtual
  // button state, so a press or release edge stays visible for one full frame.
  AppliedInput applyEvents(HalGPIO& gpio);
  // Housekeeping: auto-reconnect scheduling. `userInputDetected` resets the
  // per-boot attempt budget because the reader is being used again.
  void update(bool userInputDetected);
  // Deep-sleep teardown: releases the radio and the heap it holds.
  void shutdownForSleep();

  // Called from the NimBLE host task through the client callbacks. Only sets
  // flags and logs; never touches app state.
  void onClientDisconnected(int reason);

 private:
  BluetoothHIDManager() = default;
  ~BluetoothHIDManager() = default;
  BluetoothHIDManager(const BluetoothHIDManager&) = delete;
  BluetoothHIDManager& operator=(const BluetoothHIDManager&) = delete;

  static constexpr uint8_t MAX_REPORT_BYTES = 16;
  static constexpr uint8_t QUEUE_DEPTH = 8;
  static constexpr uint8_t MAX_SUBSCRIPTIONS = 8;
  static constexpr uint32_t WORKER_STACK_BYTES = 8192;
  static constexpr uint32_t DEFAULT_CONNECT_TIMEOUT_MS = 12000;
  static constexpr uint32_t AUTO_RECONNECT_STEP_MS = 5000;
  static constexpr uint8_t MAX_AUTO_RECONNECT_PER_BOOT = 3;

  struct HidReport {
    uint8_t length = 0;
    uint8_t data[MAX_REPORT_BYTES] = {};
  };

  enum class Job : uint8_t { None, Pair, Reconnect };

  bool ensureQueue();
  void setError(const char* message);
  bool startWorker(Job job, uint32_t scanMs);
  bool connectTo(const char* address, uint8_t addressType, const char* name, bool saveOnSuccess);
  void subscribeAll(NimBLEClient* client);
  void disconnectAll();
  void stopScanIfAny();
  void processReport(const HidReport& report, HalGPIO& gpio);
  void applyRawKey(uint8_t keycode, bool pressed, HalGPIO& gpio);
  void queuePageTurn(int8_t direction);

  static void workerTrampoline(void* param);
  void runWorker();
  static void notifyTrampoline(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify);

  bool _enabled = false;
  volatile bool _workerRunning = false;
  volatile bool _connected = false;
  volatile State _state = State::Disabled;
  volatile bool _bondChanged = false;
  Job _job = Job::None;
  uint32_t _jobScanMs = 0;

  QueueHandle_t _reportQueue = nullptr;
  class ManagerClientCallbacks* _clientCallbacks = nullptr;

  char _bondedAddress[18] = "";
  char _bondedName[33] = "";
  uint8_t _bondedAddressType = 0;
  bool _bondedLooksLikePageTurner = false;
  char _connectedName[33] = "";
  char _lastError[64] = "";

  const BleProfiles::DeviceProfile* _activeProfile = nullptr;
  uint8_t _profileActiveCode = 0;
  uint8_t _consumerActiveUsage[2] = {0, 0};
  bool _consumerActive = false;
  uint8_t _keyboardKeys[6] = {};

  volatile int8_t _pendingPageTurn = 0;
  uint8_t _autoReconnectAttempts = 0;
  uint32_t _nextAutoReconnectMs = 0;
};
