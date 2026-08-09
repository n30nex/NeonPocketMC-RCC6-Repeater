# NeonPocketMC-RCC6-Repeater

Headless MeshCore repeater and Wi-Fi MQTT observer firmware for the **Heltec RadioCore RCC6-L62 / SX1262**.

> [!WARNING]
> Experimental RCC6-only firmware. Do not flash it to RC32, RC52, or another ESP32-C6/SX1262 board. The attached RCC6 TFT is deliberately powered off to reduce RAM use, power draw, and failure surface.

This project is based on MeshCore 1.17.0 and the production MQTT observer work from [`agessaman/MeshCore`](https://github.com/agessaman/MeshCore/tree/b744b42aabb454b277fe133214c7d93d23da484b). It adds the hardware mapping already proven by the NeonPocket RCC6 companion project.

## What it does

- Runs a normal MeshCore repeater on the RCC6 SX1262 radio.
- Joins a local 2.4 GHz Wi-Fi network.
- Publishes observed mesh traffic to up to two MQTT brokers concurrently.
- Ships all 34 broker presets from the upstream observer firmware.
- Defaults slot 1 to `mqtt1.meshcore.ca` and slot 2 to `mqtt2.meshcore.ca`.
- Provides a first-boot browser setup wizard and the full serial/admin CLI.
- Keeps six MQTT slot configurations on disk; the non-PSRAM RCC6 exposes three runtime slots and permits two active connections.
- Mounts MeshCore storage fail-closed. A mount failure will not silently format identity, contacts, channels, or settings.

## First boot

1. Attach a tuned LoRa antenna before powering or transmitting.
2. Flash the application image at `0x10000`, or use the full recovery image at `0x0` only when the bootloader/partition table also needs recovery.
3. Reboot. The headless unit creates `MeshCore-Setup-XXXX` when no Wi-Fi credentials are stored.
4. Join that AP and open `http://192.168.4.1/` if the captive portal does not appear.
5. Configure Wi-Fi, radio settings, an IATA/region code, MQTT slots, and a new admin password. Save and reboot.

The setup AP is open, matching upstream behavior. Provision it at close range on a trusted network and change the default admin password immediately.

Serial recovery/configuration is available at 115200 baud over USB:

```text
set wifi.ssid Your 2.4 GHz SSID
set wifi.pwd Your WiFi password
set mqtt.iata YYZ
set mqtt1.preset meshcore-ca-1
set mqtt2.preset meshcore-ca-2
get mqtt.status
get mqtt.presets
```

Any preset can replace either active slot. For example:

```text
set mqtt2.preset meshmapper
set mqtt1.preset none
set mqtt1.preset custom
set mqtt1.server wss://broker.example:443/mqtt
```

## Built-in broker presets

The RC1 build preserves all 34 presets from the pinned observer source. The two Canadian endpoints are the only RCC6-specific default change.

| Preset | Endpoint |
|---|---|
| `analyzer-us` | `wss://mqtt-us-v1.letsmesh.net:443/mqtt` |
| `analyzer-eu` | `wss://mqtt-eu-v1.letsmesh.net:443/mqtt` |
| `nz-analyzer` | `wss://meshcore-mqtt-1.baird.io:443` |
| `meshmapper` | `wss://mqtt.meshmapper.net:443/mqtt` |
| `meshrank` | `mqtts://meshrank.net:8883` |
| `waev` | `wss://mqtt.waev.app:443/mqtt` |
| `meshomatic` | `wss://us-east.meshomatic.net:443/mqtt` |
| `cascadiamesh` | `wss://mqtt-v1.cascadiamesh.org:443/mqtt` |
| `tennmesh` | `mqtt://mqtt.tennmesh.com:1883` |
| `nashmesh` | `mqtt://mqtt.nashme.sh:1883` |
| `ctmesh` | `mqtt://mqtt.ctmesh.org:1883` |
| `chimesh` | `wss://mqtt.chimesh.org:443` |
| `meshat.se` | `wss://meshcore-mqtt.meshat.se:443` |
| `eastidahomesh` | `mqtt://live.eastidahomesh.com:1883` |
| `coloradomesh` | `wss://mqtt.meshcore.coloradomesh.org:443` |
| `dutchmeshcore-1` | `wss://collector1.dutchmeshcore.nl:443/mqtt` |
| `dutchmeshcore-2` | `wss://collector2.dutchmeshcore.nl:443/mqtt` |
| **`meshcore-ca-1`** | **`wss://mqtt1.meshcore.ca:443/mqtt`** |
| **`meshcore-ca-2`** | **`wss://mqtt2.meshcore.ca:443/mqtt`** |
| `meshcore-fi` | `wss://mc-mqtt.meshcore.fi:443/` |
| `okimesh-1` | `wss://mqtt1.okimesh.org:9002/mqtt` |
| `okimesh-2` | `wss://mqtt2.okimesh.org:9002/mqtt` |
| `inwmesh` | `mqtts://scope.inwmesh.org:8883` |
| `bostonmesh` | `wss://mqttmc01.bostonme.sh:443/mqtt` |
| `rflab` | `wss://mqtt.rflab.io:443` |
| `ipnt.uk` | `wss://mqtt.ipnt.uk:443` |
| `flmesh` | `wss://mcmqtt.jntconnections.com:443` |
| `corecomms` | `wss://mqtt.corecomms.net:443/mqtt` |
| `meshtexas` | `wss://mqtt.meshtexas.org:443/mqtt` |
| `mesh-chaun14` | `mqtt://mqtt.mesh.chaun14.fr:1884` |
| `wcmesh` | `wss://mqtt.wcmesh.com:443` |
| `atvirastinklas` | `wss://mqtt-mc.atvirastinklas.lt:443` |
| `gomesh` | `wss://mqtt.gomesh.dev:443` |
| `idahomesh` | `wss://mqtt.idahomesh.org:443/mqtt` |

Some community brokers require their own credentials or local enrollment. The setup portal and CLI expose the fields supported by each preset, and `custom` allows an operator-supplied broker.

## Flashing

Install [esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32c6/installation.html), replace `COMx`, and use exactly one command:

Application update, preserving the installed bootloader, partitions, and MeshCore data:

```powershell
esptool --chip esp32c6 --port COMx write-flash 0x10000 NeonPocketMC-RCC6-Repeater-v1.0.0-rc.1-app.bin
```

Full recovery, rewriting the bootloader/partitions/application while leaving the later SPIFFS data partition untouched:

```powershell
esptool --chip esp32c6 --port COMx write-flash 0x0 NeonPocketMC-RCC6-Repeater-v1.0.0-rc.1-full-recovery-preserves-meshcore-settings.bin
```

Do not erase the whole flash if you want to retain identity and settings. Verify downloads with `SHA256SUMS.txt`.

## Source and scope

- MeshCore base: 1.17.0.
- MQTT observer base: `agessaman/MeshCore` `observer-firmware` at `b744b42aabb454b277fe133214c7d93d23da484b`.
- RCC6 hardware mapping provenance: the separately tested `NeonPocketMC-RCC6` companion project.
- Release target: `heltec_rcc6_repeater_observer_mqtt` only.
- License: MIT; dependency notices and licenses remain in the source tree.

The RC1 release is intentionally marked as a prerelease until it receives an RCC6 on-device Wi-Fi, MQTT, LoRa TX/RX, and long-idle smoke test.
