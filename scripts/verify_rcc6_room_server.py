#!/usr/bin/env python3
"""Static contract checks for the RCC6 room-server release profiles."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def section(text: str, name: str) -> str:
    marker = f"[{name}]"
    if marker not in text:
        raise AssertionError(f"missing PlatformIO section {marker}")
    body = text.split(marker, 1)[1]
    return body.split("\n[", 1)[0]


def require(text: str, *needles: str) -> None:
    for needle in needles:
        if needle not in text:
            raise AssertionError(f"missing required contract: {needle}")


def main() -> None:
    ini = read("variants/heltec_rcc6/platformio.ini")
    expected_envs = (
        "env:heltec_rcc6_room_server_minimal_headless",
        "env:heltec_rcc6_room_server_minimal_tft",
        "env:heltec_rcc6_room_server_full_headless",
        "env:heltec_rcc6_room_server_full_tft",
    )
    for env in expected_envs:
        section(ini, env)

    common = section(ini, "heltec_rcc6_room_server_common")
    full = section(ini, "heltec_rcc6_room_server_full")
    tft = section(ini, "heltec_rcc6_room_server_tft_features")
    minimal_headless = section(ini, expected_envs[0])
    full_tft = section(ini, expected_envs[3])

    require(common, "NEONPOCKET_RCC6_ROOM_SERVER=1", "NEONPOCKET_SAFE_SPIFFS_BOOTSTRAP=1",
            "+<../examples/simple_room_server>")
    require(full, "WITH_MQTT_BRIDGE=1", "WEBCONFIG_AUTO_LAN=1",
            "MQTT_DEFAULT_SLOT1_PRESET", "MQTT_DEFAULT_SLOT2_PRESET",
            "+<helpers/bridges/MQTTBridge.cpp>")
    require(tft, "DISPLAY_CLASS=NV3001BDisplay", "NV3001B_USE_FRAMEBUFFER=1",
            "NV3001B_LOGICAL_WIDTH=220", "NV3001B_LOGICAL_HEIGHT=128",
            "+<helpers/ui/NV3001BDisplay.cpp>")
    require(minimal_headless, "minimal-headless")
    if "WITH_MQTT_BRIDGE" in common or "WITH_MQTT_BRIDGE" in minimal_headless:
        raise AssertionError("minimal room base must not enable Wi-Fi/MQTT")
    require(full_tft, "full-tft-experimental", "NEONPOCKET_ROOM_SERVER_EXPERIMENTAL=1",
            "NV3001B_USE_INDEXED_FRAMEBUFFER=1", "NEONPOCKET_MEMORY_GATE_BYTES=32768")
    if ini.count("NV3001B_USE_INDEXED_FRAMEBUFFER=1") != 1 or \
            "NV3001B_USE_INDEXED_FRAMEBUFFER" in tft:
        raise AssertionError("indexed framebuffer must be scoped to full TFT only")
    require(section(ini, "env:heltec_rcc6_repeater_observer_mqtt"),
            "NEONPOCKET_RCC6_REPEATER=1", "+<../examples/simple_repeater>")

    main_cpp = read("examples/simple_room_server/main.cpp")
    if "SPIFFS.begin(true)" in main_cpp:
        raise AssertionError("room server must never auto-format a failed mount")
    require(main_cpp, "beginSpiffsPreservingData", "nonblank data was not formatted",
            "room identity could not be saved", "factory-reset CONFIRM",
            "WiFi.disconnect(true", "next_memory_probe = millis() + 30000")

    ui_cpp = read("examples/simple_room_server/UITask.cpp")
    boot_h = read("examples/simple_room_server/NeonPocketSplash.h")
    require(ui_cpp, "NeonPocketSplash::drawFrame", "NeonPocketSplash::FRAME_MILLIS",
            "NEONPOCKET ROOM")
    require(boot_h, "DURATION_MILLIS = 3200", "FRAME_MILLIS = 125",
            "NEONPOCKETMC", "VECTOR BOOT", "RADIO LINK", "ROOM SERVICES",
            "MESH READY", "MAGENTA", "drawPocket", "MESHCORE ROOM SERVER")
    if "Starting room server..." in main_cpp:
        raise AssertionError("generic loading frame must not precede NeonPocket boot")

    mesh_h = read("examples/simple_room_server/MyMesh.h")
    mesh_cpp = read("examples/simple_room_server/MyMesh.cpp")
    require(mesh_h, "struct RoomSnapshot", "void getRoomSnapshot",
            "command requires USB serial or the room RF protocol")
    require(mesh_cpp, "room_active_clients", "room_posts", "room_pushes",
            "NEONPOCKET_ROOM_SERVER_PROFILE", "get room.profile")

    display_h = read("src/helpers/ui/NV3001BDisplay.h")
    display_cpp = read("src/helpers/ui/NV3001BDisplay.cpp")
    require(display_h, "NV3001B_USE_FRAMEBUFFER", "NV3001B_USE_INDEXED_FRAMEBUFFER",
            "framebuffer_palette_capacity = 256", "framebuffer_band_rows = 8",
            "framebuffer_band_hashes")
    require(display_cpp, "flushFramebuffer", "hashFramebufferPixels",
            "framebufferPaletteIndex", "indexed framebuffer palette exhausted",
            "sizeof(*framebuffer)")

    web = read("webui/index.html")
    require(web, "RCC6 Ultimate", "Room clients", "Room posts",
            "Post deliveries", "room_active_clients", "room_pushes", "Build profile",
            "Mesh map", "drawNeighborMap", "renderLoadBars", "renderBrokerBars",
            "/api/neighbors", "No advertised repeater locations yet")
    for unsafe_route in ('/api/room/post', '/api/room/delete', '/api/room/client'):
        if unsafe_route in web:
            raise AssertionError(f"unsafe room mutation route present: {unsafe_route}")

    configurator = read("scripts/configure_rcc6.py")
    for env in (name.split(":", 1)[1] for name in expected_envs):
        require(configurator, env)
    require(configurator, "--list-profiles", "Minimal profiles intentionally have no Wi-Fi/MQTT")

    print("RCC6 room-server static contracts passed")


if __name__ == "__main__":
    main()
