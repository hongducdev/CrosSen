#include "BluetoothSettingsActivity.h"

#if CROSSINK_APP_CAP_BLE

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>
#include <memory>

#include "BluetoothHIDManager.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char TAG[] = "BTSET";
}  // namespace

// rowLabel needs the enum, which is private; keep the mapping inside the class
// methods instead of the anonymous namespace.
void BluetoothSettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = ROW_ENABLE;
  lastStatus[0] = '\0';
  requestUpdate();
}

void BluetoothSettingsActivity::onExit() { Activity::onExit(); }

bool BluetoothSettingsActivity::skipLoopDelay() {
  // Keep polling quickly while a scan or connect worker is running so the status
  // line follows it, instead of waiting out the idle loop delay.
  return BluetoothHIDManager::getInstance().isWorkerRunning();
}

bool BluetoothSettingsActivity::rowVisible(const Row row) const {
  const BluetoothHIDManager& bluetooth = BluetoothHIDManager::getInstance();
  switch (row) {
    case ROW_ENABLE:
      return true;
    case ROW_PAIR:
      return SETTINGS.bluetoothEnabled != 0;
    case ROW_RECONNECT:
    case ROW_FORGET:
      return bluetooth.hasBond();
    default:
      return false;
  }
}

uint8_t BluetoothSettingsActivity::buildVisibleOrder(uint8_t* out) const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < ROW_COUNT; ++i) {
    if (rowVisible(static_cast<Row>(i))) out[count++] = i;
  }
  return count;
}

void BluetoothSettingsActivity::moveSelection(const int delta) {
  uint8_t order[ROW_COUNT] = {};
  const uint8_t count = buildVisibleOrder(order);
  if (count == 0) return;

  int position = 0;
  for (uint8_t i = 0; i < count; ++i) {
    if (order[i] == selectedIndex) {
      position = i;
      break;
    }
  }
  position = (position + delta + count) % count;
  selectedIndex = order[position];
}

void BluetoothSettingsActivity::listLayout(int& top, int& rowStep) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  rowStep = UITheme::getInstance().getTheme().getListRowStep(false);
  top = header.y + header.height + metrics.verticalSpacing;
}

void BluetoothSettingsActivity::buildStatusText(char* out, const size_t outLen) const {
  BluetoothHIDManager& bluetooth = BluetoothHIDManager::getInstance();
  switch (bluetooth.state()) {
    case BluetoothHIDManager::State::Disabled:
      snprintf(out, outLen, "%s", tr(STR_DISABLED));
      break;
    case BluetoothHIDManager::State::Scanning:
      snprintf(out, outLen, "%s", tr(STR_SCANNING));
      break;
    case BluetoothHIDManager::State::Pairing:
      snprintf(out, outLen, "%s", tr(STR_PAIRING));
      break;
    case BluetoothHIDManager::State::Reconnecting:
      snprintf(out, outLen, "%s", tr(STR_RECONNECTING));
      break;
    case BluetoothHIDManager::State::Connected:
      snprintf(out, outLen, "%s: %s", tr(STR_CONNECTED), bluetooth.connectedName());
      break;
    case BluetoothHIDManager::State::Error:
      snprintf(out, outLen, "%s: %s", tr(STR_BLUETOOTH_ERROR), bluetooth.lastError());
      break;
    case BluetoothHIDManager::State::Idle:
    default:
      if (bluetooth.hasBond()) {
        snprintf(out, outLen, "%s", bluetooth.bondedName());
      } else {
        snprintf(out, outLen, "%s", tr(STR_BLUETOOTH_NO_DEVICE));
      }
      break;
  }
}

void BluetoothSettingsActivity::refreshIfStateChanged() {
  char status[sizeof(lastStatus)];
  buildStatusText(status, sizeof(status));
  if (strcmp(status, lastStatus) == 0) return;
  snprintf(lastStatus, sizeof(lastStatus), "%s", status);
  requestUpdate();
}

void BluetoothSettingsActivity::activateRow(const Row row) {
  BluetoothHIDManager& bluetooth = BluetoothHIDManager::getInstance();
  switch (row) {
    case ROW_ENABLE:
      if (SETTINGS.bluetoothEnabled != 0) {
        if (!bluetooth.disable()) {
          LOG_INF(TAG, "Bluetooth busy; retry once the scan finishes");
          return;
        }
        SETTINGS.bluetoothEnabled = 0;
        SETTINGS.saveToFile();
      } else {
        if (!bluetooth.enable()) {
          LOG_ERR(TAG, "Enable failed: %s", bluetooth.lastError());
          return;
        }
        SETTINGS.bluetoothEnabled = 1;
        SETTINGS.saveToFile();
        if (bluetooth.hasBond()) bluetooth.startReconnect();
      }
      requestUpdate();
      return;
    case ROW_PAIR:
      bluetooth.startPairNew();
      requestUpdate();
      return;
    case ROW_RECONNECT:
      bluetooth.startReconnect();
      requestUpdate();
      return;
    case ROW_FORGET:
      startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_FORGET_REMOTE),
                                                                    tr(STR_FORGET_REMOTE_CONFIRM)),
                             [this](const ActivityResult& result) {
                               if (result.isCancelled) return;
                               BluetoothHIDManager::getInstance().forgetBond();
                               SETTINGS.clearBleBondedDevice();
                               SETTINGS.saveToFile();
                               requestUpdate();
                             });
      return;
    default:
      return;
  }
}

void BluetoothSettingsActivity::loop() {
  refreshIfStateChanged();

  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  uint8_t order[ROW_COUNT] = {};
  const uint8_t count = buildVisibleOrder(order);

  int top = 0;
  int rowStep = 1;
  listLayout(top, rowStep);

  int touchedRow = 0;
  if (mappedInput.rowTouch(touchedRow, top, rowStep, count) == MappedInputManager::RowTouch::Tap && touchedRow >= 0 &&
      touchedRow < count) {
    selectedIndex = order[touchedRow];
    activateRow(static_cast<Row>(selectedIndex));
    return;
  }

  if (mappedInput.wasPressed(mappedInput.menuButton(MappedInputManager::Button::Up))) {
    moveSelection(-1);
    return;
  }
  if (mappedInput.wasPressed(mappedInput.menuButton(MappedInputManager::Button::Down))) {
    moveSelection(1);
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    activateRow(static_cast<Row>(selectedIndex));
  }
}

void BluetoothSettingsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  (void)pageHeight;

  renderer.clearScreen();

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_BLUETOOTH), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_BLUETOOTH));
  }

  uint8_t order[ROW_COUNT] = {};
  const uint8_t count = buildVisibleOrder(order);

  int top = 0;
  int rowStep = 1;
  listLayout(top, rowStep);

  const int x = metrics.contentSidePadding;
  for (uint8_t i = 0; i < count; ++i) {
    const Row row = static_cast<Row>(order[i]);
    const bool selected = order[i] == selectedIndex;
    const int y = top + i * rowStep;
    char line[96];
    if (row == ROW_ENABLE) {
      snprintf(line, sizeof(line), "%s%s: %s", selected ? "> " : "  ", tr(STR_BLUETOOTH),
               SETTINGS.bluetoothEnabled != 0 ? tr(STR_ON) : tr(STR_OFF));
    } else if (row == ROW_PAIR) {
      snprintf(line, sizeof(line), "%s%s", selected ? "> " : "  ", tr(STR_PAIR_NEW_REMOTE));
    } else if (row == ROW_RECONNECT) {
      snprintf(line, sizeof(line), "%s%s", selected ? "> " : "  ", tr(STR_RECONNECT_REMOTE));
    } else {
      snprintf(line, sizeof(line), "%s%s", selected ? "> " : "  ", tr(STR_FORGET_REMOTE));
    }
    renderer.drawText(UI_10_FONT_ID, x, y, line, true, selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }

  char status[sizeof(lastStatus)];
  buildStatusText(status, sizeof(status));
  const int statusY = top + count * rowStep + metrics.verticalSpacing * 2;
  renderer.drawText(UI_10_FONT_ID, x, statusY, status);

  const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  (void)pageWidth;

  renderer.displayBuffer();
}

#endif  // CROSSINK_APP_CAP_BLE
