#include "UITask.h"
#include "MyMesh.h"

#include <Arduino.h>
#include <string.h>

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#ifndef AUTO_OFF_MILLIS
#define AUTO_OFF_MILLIS 60000
#endif

#ifndef NEONPOCKET_ROOM_SERVER_PROFILE
#define NEONPOCKET_ROOM_SERVER_PROFILE "room-server"
#endif

#ifdef WITH_MQTT_BRIDGE
#include <helpers/esp32/WebConfigServer.h>
#endif

#define BOOT_SCREEN_MILLIS 3000

static constexpr ColorVal BRAND_CYAN = 0x07FF;
static constexpr ColorVal BRAND_COBALT = 0x225F;
static constexpr ColorVal BRAND_LIME = 0x87E0;
static constexpr ColorVal BRAND_YELLOW = 0xFFE0;
static constexpr ColorVal BRAND_WHITE = 0xFFFF;
static constexpr uint16_t ROOM_LOW_BATTERY_MV = 3450;
static constexpr uint16_t ROOM_LOW_BATTERY_CLEAR_MV = 3600;

static void drawBrandMark(DisplayDriver* display, int x, int y, uint8_t phase) {
  if (phase >= 1) {
    display->setColor(BRAND_CYAN);
    display->drawRect(x, y + 8, 40, 24);
  }
  if (phase >= 2) {
    display->setColor(BRAND_COBALT);
    display->fillRect(x + 11, y + 26, 18, 2);
    display->fillRect(x + 13, y + 23, 3, 2);
    display->fillRect(x + 15, y + 20, 3, 2);
    display->fillRect(x + 17, y + 17, 3, 2);
    display->fillRect(x + 21, y + 17, 3, 2);
    display->fillRect(x + 23, y + 20, 3, 2);
    display->fillRect(x + 25, y + 23, 3, 2);
  }
  if (phase >= 3) {
    display->setColor(BRAND_LIME);
    display->fillRect(x + 18, y + 13, 5, 5);
    display->fillRect(x + 8, y + 24, 5, 5);
    display->fillRect(x + 28, y + 24, 5, 5);
  }
  if (phase >= 4) {
    display->setColor(BRAND_CYAN);
    display->fillRect(x + 7, y + 3, 4, 2);
    display->fillRect(x + 19, y, 2, 5);
    display->fillRect(x + 30, y + 2, 3, 3);
  }
}

void UITask::begin(MyMesh* mesh, NodePrefs* node_prefs,
                   const char* build_date, const char* firmware_version) {
  _mesh = mesh;
  _node_prefs = node_prefs;
  _prevBtnState = HIGH;
  _started_at = millis();
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _display->turnOn();

  char version[24];
  snprintf(version, sizeof(version), "%s", firmware_version);
  char* dash = strchr(version, '-');
  if (dash) *dash = 0;
  snprintf(_version_info, sizeof(_version_info), "%s | %s", version, build_date);
}

void UITask::renderCard(int x, int width, const char* label,
                        const char* value, ColorVal value_color) {
  _display->setColor(UIColor::secondary_txt);
  _display->drawRect(x, 30, width, 38);
  _display->setCursor(x + 4, 33);
  _display->print(label);
  _display->setColor(value_color);
  _display->setCursor(x + 4, 49);
  _display->print(value);
}

void UITask::renderCurrScreen() {
  const unsigned long elapsed = millis() - _started_at;
  if (elapsed < BOOT_SCREEN_MILLIS) {
    uint8_t phase = 1 + elapsed / 180;
    if (phase > 4) phase = 4;
    const int progress_width = 164 * elapsed / BOOT_SCREEN_MILLIS;
    drawBrandMark(_display, (_display->width() - 40) / 2, 6, phase);
    if (elapsed >= 240) {
      _display->setTextSize(2);
      _display->setColor(BRAND_LIME);
      _display->drawTextCentered(_display->width() / 2, 43, "NEONPOCKETMC");
    }
    if (elapsed >= 480) {
      _display->setTextSize(1);
      _display->setColor(BRAND_WHITE);
      _display->drawTextCentered(_display->width() / 2, 65, "MESHCORE ROOM SERVER");
    }
    _display->setColor(BRAND_COBALT);
    _display->drawRect(26, 82, 168, 8);
    _display->setColor(BRAND_YELLOW);
    _display->fillRect(28, 84, progress_width, 4);
    _display->setTextSize(2);
    _display->setTextSize(1);
    _display->setColor(BRAND_WHITE);
    _display->drawTextCentered(_display->width() / 2, 99, _version_info);
    _display->setColor(BRAND_CYAN);
    _display->drawTextCentered(_display->width() / 2, 114, NEONPOCKET_ROOM_SERVER_PROFILE);
    return;
  }

#ifdef WITH_WEBCONFIG
  if (WebConfigServer::isRebootPending()) {
    _display->setColor(UIColor::corp_blue);
    _display->fillRect(0, 0, _display->width(), 18);
    _display->setColor(UIColor::title_txt);
    _display->setCursor(5, 3);
    _display->print("ROOM SERVER");
    _display->setColor(UIColor::corp_blue);
    _display->drawTextCentered(_display->width() / 2, 48, "CONFIG SAVED");
    _display->setColor(UIColor::primary_txt);
    _display->drawTextCentered(_display->width() / 2, 70, "Rebooting...");
    return;
  }

  char setup_ssid[33], setup_ip[16];
  if (WebConfigServer::getSetupInfo(setup_ssid, sizeof(setup_ssid),
                                    setup_ip, sizeof(setup_ip))) {
    _display->setColor(UIColor::corp_blue);
    _display->fillRect(0, 0, _display->width(), 18);
    _display->setColor(UIColor::title_txt);
    _display->setCursor(5, 3);
    _display->print("ROOM WIFI SETUP");
    _display->setColor(UIColor::primary_txt);
    _display->setCursor(5, 27);
    _display->print("Join WiFi:");
    _display->setColor(UIColor::warning_txt);
    _display->setCursor(12, 43);
    _display->print(setup_ssid);
    _display->setColor(UIColor::primary_txt);
    _display->setCursor(5, 66);
    _display->print("Then open:");
    _display->setColor(UIColor::corp_blue);
    _display->setCursor(12, 82);
    _display->print(setup_ip);
    _display->setColor(UIColor::secondary_txt);
    _display->setCursor(5, 106);
    _display->print("Change the admin password");
    return;
  }
#endif

  RoomSnapshot room;
  _mesh->getRoomSnapshot(room);
  if (room.batt_mv != 0) {
    if (!_battery_low && room.batt_mv <= ROOM_LOW_BATTERY_MV) _battery_low = true;
    else if (_battery_low && room.batt_mv >= ROOM_LOW_BATTERY_CLEAR_MV) _battery_low = false;
  }
  char value[48];

  _display->setColor(UIColor::corp_blue);
  _display->fillRect(0, 0, _display->width(), 16);
  _display->setColor(UIColor::title_txt);
  _display->setCursor(4, 1);
  _display->print("NEONPOCKET ROOM");
#ifdef NEONPOCKET_ROOM_SERVER_EXPERIMENTAL
  const char* experimental = "EXP";
  _display->setCursor(_display->width() - _display->getTextWidth(experimental) - 4, 1);
  _display->print(experimental);
#endif

  _display->setColor(UIColor::primary_txt);
  _display->setCursor(4, 17);
  _display->print(_node_prefs->node_name);

  snprintf(value, sizeof(value), "%u/%u", (unsigned)room.active_clients,
           (unsigned)room.clients);
  renderCard(0, 68, "CLIENTS", value, UIColor::corp_blue);
  snprintf(value, sizeof(value), "%u", (unsigned)room.posts);
  renderCard(76, 68, "POSTS", value, UIColor::primary_txt);
  snprintf(value, sizeof(value), "%lu", (unsigned long)room.rf_rx);
  renderCard(152, 68, "RF RX", value, room.rf_errors ? UIColor::warning_txt
                                                    : UIColor::primary_txt);

  _display->setTextSize(1);
  _display->setColor(UIColor::primary_txt);
  _display->setCursor(3, 72);
  if (room.rf_rx) {
    snprintf(value, sizeof(value), "RF %d dBm  SNR %.1f  age %lus",
             room.rssi, room.snr, (unsigned long)room.last_rx_age_s);
  } else {
    snprintf(value, sizeof(value), "RF waiting  noise %d dBm", room.noise);
  }
  _display->print(value);

  _display->setCursor(3, 86);
  snprintf(value, sizeof(value), "TXQ %u ERR %04X BAT %.2fV%s",
           (unsigned)room.tx_queue, (unsigned)room.error_flags,
           room.batt_mv / 1000.0f, _battery_low ? " LOW" : "");
  _display->setColor(room.error_flags || _battery_low ? UIColor::warning_txt
                                                      : UIColor::primary_txt);
  _display->print(value);

#ifdef WITH_MQTT_BRIDGE
  _display->setCursor(3, 100);
  _display->setColor(UIColor::primary_txt);
  snprintf(value, sizeof(value), "WiFi %d dBm  MQTT %u/%u", room.wifi_rssi,
           (unsigned)room.mqtt_slots_ok, (unsigned)room.mqtt_slots_total);
  _display->print(value);

  _display->setCursor(3, 114);
  _display->setColor(room.heap_free < 50000 ? UIColor::warning_txt
                                            : UIColor::secondary_txt);
  snprintf(value, sizeof(value), "Heap %luK min %luK max %luK",
           (unsigned long)(room.heap_free / 1024),
           (unsigned long)(room.heap_min / 1024),
           (unsigned long)(room.heap_max_alloc / 1024));
  _display->print(value);
#else
  _display->setCursor(3, 100);
  _display->setColor(UIColor::secondary_txt);
  snprintf(value, sizeof(value), "Pushed %u  profile minimal", (unsigned)room.pushes);
  _display->print(value);
  _display->setCursor(3, 114);
  snprintf(value, sizeof(value), "Heap %luK min %luK",
           (unsigned long)(room.heap_free / 1024),
           (unsigned long)(room.heap_min / 1024));
  _display->print(value);
#endif
}

void UITask::loop() {
#ifdef PIN_USER_BTN
  if (millis() >= _next_read) {
    const int btn_state = digitalRead(PIN_USER_BTN);
    if (btn_state != _prevBtnState) {
      if (btn_state == USER_BTN_PRESSED) {
        if (!_display->isOn()) _display->turnOn();
        _auto_off = millis() + AUTO_OFF_MILLIS;
      }
      _prevBtnState = btn_state;
    }
    _next_read = millis() + 100;
  }
#endif

#ifdef WITH_WEBCONFIG
  if (WebConfigServer::getSetupInfo(NULL, 0, NULL, 0)) {
    if (!_display->isOn()) _display->turnOn();
    _auto_off = millis() + AUTO_OFF_MILLIS;
  }
#endif

  if (!_display->isOn()) return;
  if (millis() >= _next_refresh) {
    _display->startFrame();
    renderCurrScreen();
    _display->endFrame();
    _next_refresh = millis() + 1000;
  }
  if ((int32_t)(millis() - _auto_off) >= 0) _display->turnOff();
}
