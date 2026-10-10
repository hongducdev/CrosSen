#include "BluetoothHIDManager.h"

#include <Arduino.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <Logging.h>
#include <NimBLEDevice.h>

#include <cstring>
#include <new>

#include "DeviceProfiles.h"

namespace {

constexpr char TAG[] = "BT";
constexpr uint16_t HID_SERVICE_UUID = 0x1812;

void copyBounded(char* dst, const size_t dstLen, const char* src) {
  if (dst == nullptr || dstLen == 0) return;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  const size_t n = strnlen(src, dstLen - 1);
  memcpy(dst, src, n);
  dst[n] = '\0';
}

void copyBounded(char* dst, const size_t dstLen, const std::string& src) {
  if (dst == nullptr || dstLen == 0) return;
  const size_t n = src.size() < dstLen - 1 ? src.size() : dstLen - 1;
  memcpy(dst, src.data(), n);
  dst[n] = '\0';
}

// HID keyboard/consumer usage -> raw HalGPIO button. Returns 0xFF when the key
// has no button meaning (so arbitrary typing cannot move the UI).
uint8_t rawButtonForKeyboardKey(const uint8_t keycode) {
  switch (keycode) {
    case BleProfiles::KEY_PAGE_UP:
    case BleProfiles::KEY_UP_ARROW:
    case BleProfiles::KEY_VOLUME_UP:
      return HalGPIO::BTN_UP;
    case BleProfiles::KEY_PAGE_DOWN:
    case BleProfiles::KEY_DOWN_ARROW:
    case BleProfiles::KEY_VOLUME_DOWN:
      return HalGPIO::BTN_DOWN;
    case BleProfiles::KEY_LEFT_ARROW:
      return HalGPIO::BTN_LEFT;
    case BleProfiles::KEY_RIGHT_ARROW:
      return HalGPIO::BTN_RIGHT;
    case BleProfiles::KEY_ENTER:
    case BleProfiles::KEY_SPACE:
      return HalGPIO::BTN_CONFIRM;
    case BleProfiles::KEY_ESCAPE:
    case BleProfiles::KEY_BACKSPACE:
      return HalGPIO::BTN_BACK;
    default:
      return 0xFF;
  }
}

bool codeListContains(const uint8_t* codes, const uint8_t count, const uint8_t code) {
  for (uint8_t i = 0; i < count; ++i) {
    if (codes[i] == code) return true;
  }
  return false;
}

}  // namespace

// NimBLE calls these from its host task. They must stay flag/log only.
class ManagerClientCallbacks final : public NimBLEClientCallbacks {
 public:
  explicit ManagerClientCallbacks(BluetoothHIDManager& manager) : _manager(manager) {}

  void onDisconnect(NimBLEClient* client, int reason) override {
    (void)client;
    _manager.onClientDisconnected(reason);
  }

 private:
  BluetoothHIDManager& _manager;
};

BluetoothHIDManager& BluetoothHIDManager::getInstance() {
  static BluetoothHIDManager instance;
  return instance;
}

bool BluetoothHIDManager::ensureQueue() {
  if (_reportQueue != nullptr) return true;
  _reportQueue = xQueueCreate(QUEUE_DEPTH, sizeof(HidReport));
  if (_reportQueue == nullptr) {
    setError("queue alloc");
    LOG_ERR(TAG, "Failed to create the HID report queue");
    return false;
  }
  return true;
}

void BluetoothHIDManager::setError(const char* message) {
  copyBounded(_lastError, sizeof(_lastError), message);
  _state = State::Error;
  LOG_ERR(TAG, "%s", message == nullptr ? "error" : message);
}

bool BluetoothHIDManager::enable() {
  if (_enabled) return true;
  if (!ensureQueue()) return false;

  if (!NimBLEDevice::init("CrossInk")) {
    setError("BLE init failed");
    return false;
  }
  NimBLEDevice::setPower(9);  // +9 dBm
  // Just Works bonding: the cheap remotes this targets have no input/output
  // capability, so no passkey or numeric comparison can be performed.
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityAuth(true, false, false);

  if (_clientCallbacks == nullptr) {
    _clientCallbacks = new (std::nothrow) ManagerClientCallbacks(*this);
    if (_clientCallbacks == nullptr) {
      NimBLEDevice::deinit(true);
      setError("callback alloc failed");
      return false;
    }
  }

  _enabled = true;
  _connected = false;
  _connectedName[0] = '\0';
  _activeProfile = nullptr;
  _profileActiveCode = 0;
  _consumerActive = false;
  memset(_keyboardKeys, 0, sizeof(_keyboardKeys));
  _autoReconnectAttempts = 0;
  _nextAutoReconnectMs = 0;
  _lastError[0] = '\0';
  _state = State::Idle;
  xQueueReset(_reportQueue);
  LOG_INF(TAG, "Bluetooth enabled, free heap %u", static_cast<unsigned>(ESP.getFreeHeap()));
  return true;
}

void BluetoothHIDManager::stopScanIfAny() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan != nullptr && scan->isScanning()) {
    scan->stop();
  }
}

void BluetoothHIDManager::disconnectAll() {
  const std::vector<NimBLEClient*> clients = NimBLEDevice::getConnectedClients();
  for (NimBLEClient* client : clients) {
    if (client == nullptr) continue;
    client->disconnect();
  }
  // deleteClient also releases an already-disconnected client object.
  for (size_t i = 0; i < NimBLEDevice::getCreatedClientCount(); ++i) {
    NimBLEClient* client = NimBLEDevice::getClientByHandle(i);
    if (client != nullptr) {
      NimBLEDevice::deleteClient(client);
    }
  }
  _connected = false;
  _connectedName[0] = '\0';
}

bool BluetoothHIDManager::disable() {
  if (!_enabled) return true;
  if (_workerRunning) {
    // The worker owns the radio; the caller retries once it finishes.
    LOG_INF(TAG, "Disable refused: worker running");
    return false;
  }
  stopScanIfAny();
  disconnectAll();
  NimBLEDevice::deinit(true);
  _enabled = false;
  _activeProfile = nullptr;
  _profileActiveCode = 0;
  _consumerActive = false;
  memset(_keyboardKeys, 0, sizeof(_keyboardKeys));
  _state = State::Disabled;
  LOG_INF(TAG, "Bluetooth disabled, free heap %u", static_cast<unsigned>(ESP.getFreeHeap()));
  return true;
}

void BluetoothHIDManager::shutdownForSleep() {
  if (!_enabled) return;
  if (_workerRunning) {
    LOG_INF(TAG, "Sleep: worker still running, leaving the radio to the sleep path");
    return;
  }
  if (!disable()) {
    LOG_ERR(TAG, "Sleep: Bluetooth disable failed: %s", _lastError);
  }
}

bool BluetoothHIDManager::startWorker(const Job job, const uint32_t scanMs) {
  if (!_enabled) return false;
  if (_workerRunning) return false;
  _job = job;
  _jobScanMs = scanMs;
  _workerRunning = true;
  TaskHandle_t handle = nullptr;
  if (xTaskCreate(&BluetoothHIDManager::workerTrampoline, "bt_hid", WORKER_STACK_BYTES, this, 1, &handle) != pdPASS) {
    _workerRunning = false;
    _job = Job::None;
    setError("worker task alloc failed");
    return false;
  }
  return true;
}

bool BluetoothHIDManager::startPairNew(const uint32_t scanMs) { return startWorker(Job::Pair, scanMs); }

bool BluetoothHIDManager::startReconnect(const uint32_t scanMs) {
  if (!hasBond()) return false;
  return startWorker(Job::Reconnect, scanMs);
}

void BluetoothHIDManager::forgetBond() {
  NimBLEDevice::deleteAllBonds();
  clearBondedDevice();
  _bondChanged = true;
  LOG_INF(TAG, "Bond forgotten");
}

void BluetoothHIDManager::setBondedDevice(const char* address, const char* name, const uint8_t addressType) {
  copyBounded(_bondedAddress, sizeof(_bondedAddress), address);
  copyBounded(_bondedName, sizeof(_bondedName), name);
  _bondedAddressType = addressType;
  _bondedLooksLikePageTurner = BleProfiles::findByName(_bondedName) != nullptr;
}

void BluetoothHIDManager::clearBondedDevice() {
  _bondedAddress[0] = '\0';
  _bondedName[0] = '\0';
  _bondedAddressType = 0;
  _bondedLooksLikePageTurner = false;
}

void BluetoothHIDManager::onClientDisconnected(const int reason) {
  // Runs on the NimBLE host task: flags and logs only.
  _connected = false;
  _connectedName[0] = '\0';
  _activeProfile = nullptr;
  _profileActiveCode = 0;
  _consumerActive = false;
  memset(_keyboardKeys, 0, sizeof(_keyboardKeys));
  if (_enabled && _state == State::Connected) {
    _state = State::Idle;
  }
  LOG_INF(TAG, "Disconnected (reason %d)", reason);
}

void BluetoothHIDManager::notifyTrampoline(NimBLERemoteCharacteristic* characteristic, uint8_t* data,
                                           const size_t length, const bool isNotify) {
  (void)characteristic;
  (void)isNotify;
  BluetoothHIDManager& manager = BluetoothHIDManager::getInstance();
  if (!manager._enabled || manager._reportQueue == nullptr || data == nullptr || length == 0) return;

  HidReport report;
  report.length = static_cast<uint8_t>(length < MAX_REPORT_BYTES ? length : MAX_REPORT_BYTES);
  memcpy(report.data, data, report.length);
  // Bounded queue: dropping a report is safer than blocking the NimBLE host task.
  (void)xQueueSend(manager._reportQueue, &report, 0);
}

void BluetoothHIDManager::subscribeAll(NimBLEClient* client) {
  if (client == nullptr) return;
  uint8_t subscribed = 0;
  const std::vector<NimBLERemoteService*>& services = client->getServices(true);
  for (NimBLERemoteService* service : services) {
    if (service == nullptr) continue;
    const std::vector<NimBLERemoteCharacteristic*>& characteristics = service->getCharacteristics(true);
    for (NimBLERemoteCharacteristic* characteristic : characteristics) {
      if (characteristic == nullptr || !characteristic->canNotify()) continue;
      if (subscribed >= MAX_SUBSCRIPTIONS) {
        LOG_INF(TAG, "Subscription cap reached (%u)", static_cast<unsigned>(MAX_SUBSCRIPTIONS));
        return;
      }
      if (characteristic->subscribe(true, &BluetoothHIDManager::notifyTrampoline, false)) {
        ++subscribed;
        LOG_INF(TAG, "Subscribed to %s", characteristic->getUUID().toString().c_str());
      }
    }
  }
  LOG_INF(TAG, "%u notification source(s) subscribed", static_cast<unsigned>(subscribed));
}

bool BluetoothHIDManager::connectTo(const char* address, const uint8_t addressType, const char* name,
                                    const bool saveOnSuccess) {
  if (address == nullptr || address[0] == '\0') {
    setError("no address");
    return false;
  }
  const NimBLEAddress bleAddress(std::string(address), addressType);
  NimBLEClient* client = NimBLEDevice::getClientByPeerAddress(bleAddress);
  if (client == nullptr) {
    client = NimBLEDevice::createClient(bleAddress);
  }
  if (client == nullptr) {
    setError("client alloc failed");
    return false;
  }
  client->setClientCallbacks(_clientCallbacks, false);
  client->setConnectTimeout(DEFAULT_CONNECT_TIMEOUT_MS);
  if (!client->connect(bleAddress)) {
    NimBLEDevice::deleteClient(client);
    setError("connect failed");
    return false;
  }

  _connected = true;
  copyBounded(_connectedName, sizeof(_connectedName), name);
  subscribeAll(client);
  _activeProfile = BleProfiles::findByName(_connectedName);
  if (saveOnSuccess) {
    setBondedDevice(address, _connectedName, addressType);
    _bondChanged = true;
  }
  _state = State::Connected;
  LOG_INF(TAG, "Connected to %s (%s)", _connectedName[0] != '\0' ? _connectedName : "unknown", address);
  return true;
}

void BluetoothHIDManager::workerTrampoline(void* param) { static_cast<BluetoothHIDManager*>(param)->runWorker(); }

void BluetoothHIDManager::runWorker() {
  // Keep the CPU at normal speed while scanning/connecting: the fork found that
  // the idle power-saving clock drop breaks BLE work on this board.
  HalPowerManager::Lock powerLock;

  const Job job = _job;
  _state = (job == Job::Pair) ? State::Pairing : State::Reconnecting;

  char address[18] = "";
  char name[33] = "";
  uint8_t addressType = 0;
  bool found = false;

  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan != nullptr) {
    scan->setActiveScan(true);  // active scan also collects scan responses (device names)
    scan->setInterval(100);
    scan->setWindow(80);
    scan->setDuplicateFilter(false);
    scan->setMaxResults(24);
    scan->clearResults();
    uint32_t seconds = _jobScanMs / 1000;
    if (seconds == 0) seconds = 10;
    LOG_INF(TAG, "%s: scanning for up to %us", job == Job::Pair ? "Pair" : "Reconnect", static_cast<unsigned>(seconds));
    scan->start(seconds, false, false);

    const NimBLEScanResults results = scan->getResults();
    const int count = results.getCount();
    const NimBLEAdvertisedDevice* best = nullptr;
    const NimBLEAdvertisedDevice* singleConnectable = nullptr;
    int connectableCount = 0;
    for (int i = 0; i < count; ++i) {
      const NimBLEAdvertisedDevice* device = results.getDevice(static_cast<uint32_t>(i));
      if (device == nullptr) continue;
      const std::string deviceAddress = device->getAddress().toString();
      if (job == Job::Reconnect && _bondedAddress[0] != '\0' && deviceAddress == _bondedAddress) {
        best = device;
        break;
      }
      if (job == Job::Pair && _bondedAddress[0] != '\0' && deviceAddress == _bondedAddress) {
        continue;  // pairing a new remote: skip the one we already remember
      }
      if (!device->isConnectable()) continue;
      ++connectableCount;
      singleConnectable = device;
      const std::string deviceName = device->haveName() ? device->getName() : std::string();
      if (job == Job::Pair && BleProfiles::findByName(deviceName.c_str()) != nullptr) {
        best = device;
        break;
      }
      if (device->haveServiceUUID() && device->isAdvertisingService(NimBLEUUID(HID_SERVICE_UUID)) && best == nullptr) {
        best = device;
      }
    }
    // Fail safe: only adopt a nameless device when exactly one candidate exists.
    if (best == nullptr && job == Job::Pair && connectableCount == 1) {
      best = singleConnectable;
    }
    if (best != nullptr) {
      found = true;
      copyBounded(address, sizeof(address), best->getAddress().toString());
      if (best->haveName()) copyBounded(name, sizeof(name), best->getName());
      addressType = best->getAddress().getType();
    }
    scan->stop();
    scan->clearResults();
  }

  if (found) {
    connectTo(address, addressType, name, job == Job::Pair);
  } else if (job == Job::Reconnect) {
    setError("remote not found");
  } else {
    setError("no page turner found");
  }

  if (_state != State::Connected && _state != State::Error) {
    _state = State::Idle;
  }
  _workerRunning = false;
  _job = Job::None;
  vTaskDelete(nullptr);
}

void BluetoothHIDManager::applyRawKey(const uint8_t keycode, const bool pressed, HalGPIO& gpio) {
  const uint8_t button = rawButtonForKeyboardKey(keycode);
  if (button == 0xFF) return;  // unmapped key: ignore instead of guessing
  gpio.setVirtualButtonState(button, pressed);
}

void BluetoothHIDManager::queuePageTurn(const int8_t direction) {
  if (direction == 0) return;
  _pendingPageTurn = direction;
}

void BluetoothHIDManager::processReport(const HidReport& report, HalGPIO& gpio) {
  if (report.length == 0) return;

  // 1) A known page-turner profile: these reports put their codes anywhere in
  //    the frame, so scan every byte and treat a held code as one action.
  if (_activeProfile != nullptr) {
    uint8_t matchedCode = 0;
    int8_t direction = 0;
    for (uint8_t i = 0; i < report.length && matchedCode == 0; ++i) {
      const uint8_t byte = report.data[i];
      if (byte == 0) continue;
      if (codeListContains(_activeProfile->forwardCodes, _activeProfile->forwardCount, byte)) {
        matchedCode = byte;
        direction = 1;
      } else if (codeListContains(_activeProfile->backCodes, _activeProfile->backCount, byte)) {
        matchedCode = byte;
        direction = -1;
      }
    }
    if (matchedCode == 0) {
      _profileActiveCode = 0;  // released
      return;
    }
    if (matchedCode == _profileActiveCode) return;  // still held: one action per press
    _profileActiveCode = matchedCode;
    queuePageTurn(direction);
    return;
  }

  // 2) Boot-protocol keyboard report: modifiers + up to six keycodes.
  if (report.length >= 8) {
    uint8_t keys[6] = {};
    for (uint8_t i = 0; i < 6; ++i) keys[i] = report.data[2 + i];

    for (uint8_t i = 0; i < 6; ++i) {
      const uint8_t previous = _keyboardKeys[i];
      if (previous == 0) continue;
      bool stillHeld = false;
      for (uint8_t j = 0; j < 6; ++j) {
        if (keys[j] == previous) {
          stillHeld = true;
          break;
        }
      }
      if (!stillHeld) applyRawKey(previous, false, gpio);
    }
    for (uint8_t i = 0; i < 6; ++i) {
      const uint8_t key = keys[i];
      if (key == 0) continue;
      bool alreadyHeld = false;
      for (uint8_t j = 0; j < 6; ++j) {
        if (_keyboardKeys[j] == key) {
          alreadyHeld = true;
          break;
        }
      }
      if (!alreadyHeld) applyRawKey(key, true, gpio);
    }
    memcpy(_keyboardKeys, keys, sizeof(keys));
    return;
  }

  // 3) Consumer-control report: a 16-bit usage, little endian, or one byte.
  uint16_t usage = 0;
  if (report.length >= 2) {
    usage = static_cast<uint16_t>(report.data[0]) | (static_cast<uint16_t>(report.data[1]) << 8);
  } else {
    usage = report.data[0];
  }
  if (usage == 0) {
    _consumerActive = false;
    return;
  }
  if (_consumerActive && usage == _consumerActiveUsage[0]) return;  // still held
  _consumerActive = true;
  _consumerActiveUsage[0] = static_cast<uint8_t>(usage & 0xFF);
  switch (usage) {
    case BleProfiles::CONSUMER_PAGE_DOWN:
    case BleProfiles::CONSUMER_NEXT_TRACK:
    case BleProfiles::CONSUMER_SCAN_NEXT:
      queuePageTurn(1);
      break;
    case BleProfiles::CONSUMER_PAGE_UP:
    case BleProfiles::CONSUMER_PREV_TRACK:
    case BleProfiles::CONSUMER_SCAN_PREV:
      queuePageTurn(-1);
      break;
    case BleProfiles::CONSUMER_PLAY_PAUSE:
      queuePageTurn(1);
      break;
    default:
      break;
  }
}

BluetoothHIDManager::AppliedInput BluetoothHIDManager::applyEvents(HalGPIO& gpio) {
  AppliedInput applied;
  if (!_enabled || _reportQueue == nullptr) return applied;

  // One report per frame: a press and its release land in different frames, so
  // no edge can be coalesced away. Latching here keeps the edge visible for the
  // whole frame instead of being cleared by the next gpio.update().
  HidReport report;
  if (xQueueReceive(_reportQueue, &report, 0) == pdTRUE) {
    processReport(report, gpio);
    gpio.latchVirtualButtons();
  }

  applied.pageTurn = _pendingPageTurn;
  _pendingPageTurn = 0;
  return applied;
}

void BluetoothHIDManager::update(const bool userInputDetected) {
  if (!_enabled) return;
  const uint32_t now = millis();

  if (userInputDetected) {
    // The reader is being used again: reset the attempt budget.
    _autoReconnectAttempts = 0;
  }
  if (_connected) {
    _autoReconnectAttempts = 0;
    return;
  }
  if (_workerRunning || !hasBond()) return;
  if (_autoReconnectAttempts >= MAX_AUTO_RECONNECT_PER_BOOT) return;
  if (_nextAutoReconnectMs != 0 && now < _nextAutoReconnectMs) return;

  ++_autoReconnectAttempts;
  _nextAutoReconnectMs = now + AUTO_RECONNECT_STEP_MS * _autoReconnectAttempts;
  LOG_INF(TAG, "Auto-reconnect attempt %u/%u", static_cast<unsigned>(_autoReconnectAttempts),
          static_cast<unsigned>(MAX_AUTO_RECONNECT_PER_BOOT));
  startReconnect();
}
