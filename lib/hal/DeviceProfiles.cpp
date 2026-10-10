#include "DeviceProfiles.h"

#include <cctype>
#include <cstring>

namespace BleProfiles {

namespace {

// Case-insensitive substring test that never reads past either buffer.
bool containsIgnoreCase(const char* haystack, const char* needle) {
  if (haystack == nullptr || needle == nullptr || *needle == '\0') return false;
  const size_t needleLen = strlen(needle);
  for (const char* start = haystack; *start != '\0'; ++start) {
    size_t i = 0;
    while (i < needleLen && start[i] != '\0' &&
           std::tolower(static_cast<unsigned char>(start[i])) == std::tolower(static_cast<unsigned char>(needle[i]))) {
      ++i;
    }
    if (i == needleLen) return true;
  }
  return false;
}

}  // namespace

// Keep the table small and ordered: the first name match wins, so put the more
// specific names before the generic ones.
const DeviceProfile KNOWN_PROFILES[] = {
    {"Free2", FREE2_FORWARD, sizeof(FREE2_FORWARD), FREE2_BACK, sizeof(FREE2_BACK)},
    {"Free3", FREE2_FORWARD, sizeof(FREE2_FORWARD), FREE2_BACK, sizeof(FREE2_BACK)},
    {"Game Brick", GAMEBRICK_FORWARD, sizeof(GAMEBRICK_FORWARD), GAMEBRICK_BACK, sizeof(GAMEBRICK_BACK)},
    {"GameBrick", GAMEBRICK_FORWARD, sizeof(GAMEBRICK_FORWARD), GAMEBRICK_BACK, sizeof(GAMEBRICK_BACK)},
    {"IINE", GAMEBRICK_FORWARD, sizeof(GAMEBRICK_FORWARD), GAMEBRICK_BACK, sizeof(GAMEBRICK_BACK)},
    {"Free2-M", FREEM_FORWARD, sizeof(FREEM_FORWARD), FREEM_BACK, sizeof(FREEM_BACK)},
    {"Free3-M", FREEM_FORWARD, sizeof(FREEM_FORWARD), FREEM_BACK, sizeof(FREEM_BACK)},
    {"MINI_KEYBOARD", nullptr, 0, nullptr, 0},
};

const size_t KNOWN_PROFILE_COUNT = sizeof(KNOWN_PROFILES) / sizeof(KNOWN_PROFILES[0]);

const DeviceProfile* findByName(const char* advertisedName) {
  if (advertisedName == nullptr || advertisedName[0] == '\0') return nullptr;
  for (size_t i = 0; i < KNOWN_PROFILE_COUNT; ++i) {
    const DeviceProfile& profile = KNOWN_PROFILES[i];
    if (profile.forwardCodes == nullptr) continue;  // placeholder entry
    if (containsIgnoreCase(advertisedName, profile.name)) return &profile;
  }
  return nullptr;
}

}  // namespace BleProfiles
