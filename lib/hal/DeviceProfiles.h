#pragma once

#include <cstddef>
#include <cstdint>

// Compact HID profile table for Bluetooth page turners.
//
// Ported from the hardware-validated fork
// hannah-nula/crosspoint-x3-ble-page-turner (lib/hal/DeviceProfiles.h), keeping
// only what a v1 needs: the generic keyboard and consumer-control paths plus the
// cheap page-turner families that do NOT follow either convention.
//
// Everything here is a constant table, so it lives in flash, not DRAM.
namespace BleProfiles {

// --- Keyboard page usages (HID usage page 0x07) -----------------------------
constexpr uint8_t KEY_ENTER = 0x28;
constexpr uint8_t KEY_ESCAPE = 0x29;
constexpr uint8_t KEY_BACKSPACE = 0x2A;
constexpr uint8_t KEY_SPACE = 0x2C;
constexpr uint8_t KEY_PAGE_UP = 0x4B;
constexpr uint8_t KEY_PAGE_DOWN = 0x4E;
constexpr uint8_t KEY_RIGHT_ARROW = 0x4F;
constexpr uint8_t KEY_LEFT_ARROW = 0x50;
constexpr uint8_t KEY_DOWN_ARROW = 0x51;
constexpr uint8_t KEY_UP_ARROW = 0x52;
// Some remotes send these "volume" codes in keyboard mode instead of consumer.
constexpr uint8_t KEY_VOLUME_UP = 0x80;
constexpr uint8_t KEY_VOLUME_DOWN = 0x81;

// --- Consumer page usages (HID usage page 0x0C) -----------------------------
// Cheap remotes reuse Volume Increment/Decrement as page up/down, so 0xE9/0xEA
// must map to page navigation, not to volume.
constexpr uint16_t CONSUMER_PAGE_UP = 0x00E9;
constexpr uint16_t CONSUMER_PAGE_DOWN = 0x00EA;
constexpr uint16_t CONSUMER_PLAY_PAUSE = 0x00CD;
constexpr uint16_t CONSUMER_NEXT_TRACK = 0x00B5;
constexpr uint16_t CONSUMER_PREV_TRACK = 0x00B6;
constexpr uint16_t CONSUMER_SCAN_NEXT = 0x0001;
constexpr uint16_t CONSUMER_SCAN_PREV = 0x0002;

// --- Free2/Free3 style rolling keycode families -----------------------------
// Observed while the button is held: the remote walks a family of codes.
constexpr uint8_t FREE2_FORWARD[4] = {0x1C, 0xC4, 0x6C, 0xBC};
constexpr uint8_t FREE2_BACK[4] = {0xB4, 0x0E, 0x66, 0x16};
// Free2-M / Free3-M: plain 0x02 / 0x01 in keyboard-style reports.
constexpr uint8_t FREEM_FORWARD[1] = {0x02};
constexpr uint8_t FREEM_BACK[1] = {0x01};
// IINE Game Brick: page keys in a non-standard byte, decoded by scanning.
constexpr uint8_t GAMEBRICK_FORWARD[1] = {0x07};
constexpr uint8_t GAMEBRICK_BACK[1] = {0x09};

struct DeviceProfile {
  const char* name;  // matched as a case-insensitive substring of the advertised name
  const uint8_t* forwardCodes;
  uint8_t forwardCount;
  const uint8_t* backCodes;
  uint8_t backCount;
};

// Known devices, matched by advertised-name substring (nullptr name = no match).
extern const DeviceProfile KNOWN_PROFILES[];
extern const size_t KNOWN_PROFILE_COUNT;

// Case-insensitive substring match against the advertised name. Returns nullptr
// when no profile matches.
const DeviceProfile* findByName(const char* advertisedName);

}  // namespace BleProfiles
