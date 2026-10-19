// What the portable app needs from the hardware (board) or the simulator.
#pragma once
#include <cstdint>
#include <cstddef>

struct Note { uint16_t hz; uint16_t ms; };   // hz 0 = rest

// Settings kept beside the pet state (separate NVS key).
struct Settings {
  uint32_t magic;
  uint8_t  version;
  uint8_t  sound;        // 0 on, 1 chimes only, 2 off
  uint8_t  brightness;   // 0 low, 1 medium, 2 high
  uint8_t  timeout;      // 0 = 30 s, 1 = 60 s, 2 = 2 min
  uint8_t  hasPet;
  uint8_t  bootSeen;
  uint16_t caseNo;       // case file number shown on the boot screen
  char     name[20];     // pet name (from the name generator)
};
static const uint32_t SETTINGS_MAGIC = 0x43534554;  // "CSET"

class Platform {
 public:
  virtual ~Platform() {}
  virtual uint32_t unixNow() = 0;            // UTC seconds
  virtual int32_t  utcOffset(uint32_t now) = 0;
  virtual bool     clockVerified() = 0;      // NTP answered this boot
  virtual void     setClock(uint32_t unix) = 0;
  virtual void     play(const Note* notes, int n) = 0;   // non-blocking
  virtual void     backlight(uint8_t level) = 0;         // 0 off .. 255
  virtual int      batteryPercent() = 0;                 // -1 unknown
  virtual void     savePet(const uint8_t* data, size_t len) = 0;
  virtual void     saveSettings(const Settings& s) = 0;
};
