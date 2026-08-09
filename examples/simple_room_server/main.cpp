#include <Arduino.h>   // needed for PlatformIO
#include <stdlib.h>
#include <Mesh.h>

#include "MyMesh.h"

#if defined(NEONPOCKET_RCC6_ROOM_SERVER) && defined(WITH_MQTT_BRIDGE)
  #include <WiFi.h>
#endif

#if defined(ESP32) && defined(NEONPOCKET_SAFE_SPIFFS_BOOTSTRAP)
  #include <esp_partition.h>
#endif

#ifdef ETHERNET_ENABLED
  #define ETHERNET_CLI_BANNER "MeshCore Room Server CLI"
  #include <helpers/nrf52/EthernetCLI.h>
#endif

#ifdef DISPLAY_CLASS
  #include "NeonPocketBoot.h"
  #include "UITask.h"
  static UITask ui_task(display);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) {
    delay(1000);
  }
}

#ifdef DISPLAY_CLASS
static void showFatal(DisplayDriver* display_driver, const char* line1, const char* line2) {
  if (display_driver == NULL) return;
  if (!display_driver->isOn()) display_driver->turnOn();
  if (!display_driver->isOn()) return;
  display_driver->startFrame();
  display_driver->setTextSize(1);
  display_driver->setColor(UIColor::warning_txt);
  display_driver->drawTextCentered(display_driver->width() / 2,
      display_driver->height() / 2 - 10, line1);
  display_driver->setColor(UIColor::primary_txt);
  display_driver->drawTextCentered(display_driver->width() / 2,
      display_driver->height() / 2 + 8, line2);
  display_driver->endFrame();
}
#endif

#ifdef NEONPOCKET_MEMORY_GATE_BYTES
static unsigned long next_memory_probe = 0;

static bool probeRuntimeHeadroom() {
  void* probe = malloc(NEONPOCKET_MEMORY_GATE_BYTES);
  if (probe == NULL) return false;
  free(probe);
  return true;
}
#endif

#if defined(ESP32) && defined(NEONPOCKET_SAFE_SPIFFS_BOOTSTRAP)
static bool isSpiffsPartitionErased() {
  const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  if (partition == nullptr) {
    Serial.println("FATAL storage: SPIFFS partition not found");
    return false;
  }

  static uint8_t chunk[1024];
  for (size_t offset = 0; offset < partition->size; offset += sizeof(chunk)) {
    const size_t remaining = partition->size - offset;
    const size_t length = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    if (esp_partition_read(partition, offset, chunk, length) != ESP_OK) {
      Serial.println("FATAL storage: SPIFFS partition read failed");
      return false;
    }
    for (size_t i = 0; i < length; i++) {
      if (chunk[i] != 0xFF) return false;
    }
  }
  return true;
}

static bool beginSpiffsPreservingData() {
  if (SPIFFS.begin(false)) return true;
  if (!isSpiffsPartitionErased()) {
    Serial.println("FATAL storage: mount failed; nonblank data was not formatted");
    return false;
  }
  Serial.println("SPIFFS: blank partition detected; creating filesystem");
  if (!SPIFFS.format()) {
    Serial.println("FATAL storage: blank-partition format failed");
    return false;
  }
  if (!SPIFFS.begin(false)) {
    Serial.println("FATAL storage: mount failed after blank-partition format");
    return false;
  }
  return true;
}
#endif

static char command[MAX_POST_TEXT_LEN+1];
#ifdef NEONPOCKET_RCC6_ROOM_SERVER
static unsigned long factory_reset_at = 0;
#endif
#ifdef ETHERNET_ENABLED
static char ethernet_command[MAX_POST_TEXT_LEN+1];
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  board.begin();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.begin();
#endif

#ifdef DISPLAY_CLASS
  DisplayDriver* active_display = NULL;
  if (display.begin()) {
    active_display = &display;
    active_display->startFrame(NeonPocketBoot::BG);
    NeonPocketBoot::draw(*active_display, 0, "MESHCORE ROOM SERVER",
                         FIRMWARE_VERSION);
    active_display->endFrame();
  } else {
    Serial.println("FATAL display: required display initialization failed");
  #ifdef DISPLAY_REQUIRED
    halt();
  #endif
  }
#endif

  if (!radio_init()) {
    Serial.println("FATAL radio: initialization failed");
#ifdef DISPLAY_CLASS
    showFatal(active_display, "RADIO INIT FAILED", "Reset device");
#endif
    halt();
  }

  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#elif defined(ESP32)
#ifdef NEONPOCKET_SAFE_SPIFFS_BOOTSTRAP
  if (!beginSpiffsPreservingData()) {
  #ifdef DISPLAY_CLASS
    showFatal(active_display, "STORAGE ERROR", "Data not erased");
  #endif
    halt();
  }
#else
  if (!SPIFFS.begin(false)) {
    Serial.println("FATAL storage: SPIFFS mount failed");
  #ifdef DISPLAY_CLASS
    showFatal(active_display, "STORAGE ERROR", "Data not erased");
  #endif
    halt();
  }
#endif
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#else
  #error "need to define filesystem"
#endif
  if (!store.load("_main", the_mesh.self_id)) {
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    if (!store.save("_main", the_mesh.self_id)) {
      Serial.println("FATAL storage: room identity could not be saved");
#ifdef DISPLAY_CLASS
      showFatal(active_display, "STORAGE ERROR", "Identity not saved");
#endif
      halt();
    }
  }

  Serial.print("Room ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  command[0] = 0;
#ifdef ETHERNET_ENABLED
  ethernet_command[0] = 0;
#endif

  sensors.begin();

  the_mesh.begin(fs);

#ifdef DISPLAY_CLASS
  ui_task.begin(&the_mesh, the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

#ifdef NEONPOCKET_MEMORY_GATE_BYTES
  // Wi-Fi, the authenticated Web server, and both TLS clients allocate after
  // begin() returns. Delay the first qualification probe until those services
  // have had time to settle, then repeat it once per minute.
  Serial.printf("Room server %u-byte post-service heap gate armed; free=%u min=%u max=%u\n",
                (unsigned)NEONPOCKET_MEMORY_GATE_BYTES, (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  next_memory_probe = millis() + 30000;
#endif

#ifdef ETHERNET_ENABLED
  ethernet_start_task();
#endif

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif

  board.onBootComplete();
}

void loop() {
  int len = strlen(command);
  while (Serial.available() && len < sizeof(command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
    }
    Serial.print(c);
  }
  if (len == sizeof(command)-1) {  // command buffer full
    command[sizeof(command)-1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {  // received complete line
    command[len - 1] = 0;  // replace newline with C string null terminator
    char reply[160];
    reply[0] = 0;
#ifdef NEONPOCKET_RCC6_ROOM_SERVER
    if (strcmp(command, "factory-reset CONFIRM") == 0) {
      if (the_mesh.formatFileSystem()) {
        factory_reset_at = millis() + 2000;
        strcpy(reply, "OK - storage erased; network clear and reboot scheduled");
        Serial.println("Factory reset: filesystem erased; network clear scheduled");
      } else {
        strcpy(reply, "Err - filesystem erase failed; reset cancelled");
      }
    } else
#endif
    {
#ifdef ETHERNET_ENABLED
      if (!ethernet_handle_command(command, reply)) {
        the_mesh.handleCommand(0, command, reply);
      }
#else
      the_mesh.handleCommand(0, command, reply);  // NOTE: there is no sender_timestamp via serial!
#endif
    }
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }

    command[0] = 0;  // reset command buffer
  }

#ifdef NEONPOCKET_RCC6_ROOM_SERVER
  if (factory_reset_at && (long)(millis() - factory_reset_at) >= 0) {
    factory_reset_at = 0;
    Serial.println("Factory reset: clearing stored network credentials and rebooting");
#ifdef WITH_MQTT_BRIDGE
    the_mesh.setBridgeState(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true /*radio off*/, true /*erase stored AP from NVS*/);
#endif
    delay(100);
    board.reboot();
    return;
  }
#endif

#ifdef ETHERNET_ENABLED
  ethernet_loop_maintain();
  if (ethernet_read_line(ethernet_command, sizeof(ethernet_command))) {
    char reply[160];
    reply[0] = 0;
    if (!ethernet_handle_command(ethernet_command, reply)) {
      the_mesh.handleCommand(0, ethernet_command, reply);
    }
    ethernet_send_reply(reply);
    ethernet_command[0] = 0;
  }
#endif

  the_mesh.loop();
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();
#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.loop();
#endif

#ifdef NEONPOCKET_MEMORY_GATE_BYTES
  const unsigned long memory_now = millis();
  if ((long)(memory_now - next_memory_probe) >= 0) {
    next_memory_probe = memory_now + 60000;
    if (!probeRuntimeHeadroom()) {
      Serial.printf("FATAL memory: runtime %u-byte headroom gate failed; free=%u min=%u max=%u\n",
                    (unsigned)NEONPOCKET_MEMORY_GATE_BYTES, (unsigned)ESP.getFreeHeap(),
                    (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  #ifdef DISPLAY_CLASS
      showFatal(&display, "MEMORY GATE FAILED", "Activity halted");
  #endif
      halt();
    }
  }
#endif
}
