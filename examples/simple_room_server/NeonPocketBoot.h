#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <stdint.h>

namespace NeonPocketBoot {

static constexpr unsigned long DURATION_MILLIS = 3200;
static constexpr unsigned long FRAME_MILLIS = 120;
static constexpr ColorVal BG = 0x0000;
static constexpr ColorVal CYAN = 0x07FF;
static constexpr ColorVal COBALT = 0x225F;
static constexpr ColorVal LIME = 0x87E0;
static constexpr ColorVal MAGENTA = 0xF81F;
static constexpr ColorVal YELLOW = 0xFFE0;
static constexpr ColorVal WHITE = 0xFFFF;

struct Star {
  uint8_t x;
  uint8_t y;
};

static inline void drawPocket(DisplayDriver& display, uint8_t reveal) {
  const int x = 90;
  const int y = 7;
  display.setColor(CYAN);
  if (reveal > 0) display.fillRect(x, y + 5, 20, 2);
  if (reveal > 1) display.fillRect(x + 20, y + 5, 20, 2);
  if (reveal > 2) display.fillRect(x, y + 5, 2, 18);
  if (reveal > 3) display.fillRect(x + 38, y + 5, 2, 18);
  if (reveal > 4) display.fillRect(x, y + 23, 7, 2);
  if (reveal > 5) display.fillRect(x + 33, y + 23, 7, 2);
  if (reveal > 6) display.fillRect(x + 5, y + 25, 7, 2);
  if (reveal > 7) display.fillRect(x + 28, y + 25, 7, 2);
  if (reveal > 8) display.fillRect(x + 10, y + 27, 20, 2);

  if (reveal > 4) {
    display.setColor(COBALT);
    for (int i = 0; i < 6; ++i) {
      display.fillRect(x + 19 - i * 3, y + 11 + i * 2, 2, 2);
      display.fillRect(x + 21 + i * 3, y + 11 + i * 2, 2, 2);
    }
  }
  if (reveal > 7) {
    display.setColor(LIME);
    display.fillRect(x + 18, y + 8, 5, 5);
    display.fillRect(x + 7, y + 20, 4, 4);
    display.fillRect(x + 29, y + 20, 4, 4);
  }
}

static inline void draw(DisplayDriver& display, unsigned long elapsed,
                        const char* role, const char* version) {
  static const Star stars[] = {
      {7, 8}, {25, 18}, {48, 6}, {69, 27}, {151, 12}, {176, 25},
      {207, 8}, {12, 58}, {34, 75}, {190, 67}, {213, 48}, {157, 83}};
  const unsigned long bounded = elapsed < DURATION_MILLIS
      ? elapsed : DURATION_MILLIS;
  const uint8_t frame = elapsed / FRAME_MILLIS;
  const uint8_t reveal = bounded >= 900 ? 9 : bounded / 100;
  const uint8_t twinkle = frame % 7;  // first seven stars share the logo bands

  for (uint8_t i = 0; i < sizeof(stars) / sizeof(stars[0]); ++i) {
    display.setColor(i == twinkle ? WHITE : COBALT);
    const int size = i == twinkle ? 2 : 1;
    display.fillRect(stars[i].x, stars[i].y, size, size);
  }

  // Static raster-floor accents keep most framebuffer bands unchanged.
  display.setColor(COBALT);
  for (int x = 4; x < 216; x += 12) display.fillRect(x, 88, 7, 1);
  for (int x = 9; x < 216; x += 18) display.fillRect(x, 92, 10, 1);

  drawPocket(display, reveal);

  // A packet spark runs through the revealed mesh.
  if (bounded > 700) {
    const uint8_t packet = (frame / 2) % 8;
    display.setColor(YELLOW);
    display.fillRect(108 + (packet < 4 ? -packet * 3 : (packet - 4) * 3),
                     20 + (packet % 4) * 2, 3, 2);
  }

  // A narrow highlight and raster beam provide motion without full-frame churn.
  if (bounded > 300) {
    const int shine_x = 92 + ((frame * 3) % 34);
    display.setColor(WHITE);
    display.fillRect(shine_x, 14, 4, 2);
    const int beam_y = 34 + ((frame * 5) % 50);
    display.setColor(COBALT);
    display.fillRect(7, beam_y, 206, 1);
    display.setColor(CYAN);
    display.fillRect(17, beam_y + 2, 186, 1);
  }

  display.setTextSize(2);
  if (bounded < 650) {
    display.setColor(MAGENTA);
    display.drawTextCentered(108, 45, "NEONPOCKETMC");
    display.setColor(CYAN);
    display.drawTextCentered(112, 45, "NEONPOCKETMC");
  }
  display.setColor(LIME);
  display.drawTextCentered(110, 45, "NEONPOCKETMC");

  display.setTextSize(1);
  display.setColor(WHITE);
  display.setCursor(4, 4);
  display.print(version);
  display.setColor(WHITE);
  display.drawTextCentered(110, 69, role);

  const char* status = bounded < 800 ? "VECTOR BOOT" :
      bounded < 1600 ? "LINKING MESH" :
      bounded < 2400 ? "WAKING RADIO" : "NEON ONLINE";
  display.setColor(CYAN);
  display.drawTextCentered(110, 98, status);

  const int progress = 188 * bounded / DURATION_MILLIS;
  display.setColor(COBALT);
  display.drawRect(14, 113, 192, 8);
  display.setColor(YELLOW);
  if (progress > 0) display.fillRect(16, 115, progress, 4);
}

}  // namespace NeonPocketBoot
