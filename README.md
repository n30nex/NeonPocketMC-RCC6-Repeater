<p align="center">
  <img src="https://raw.githubusercontent.com/n30nex/NeonPocketMC/main/branding/neonpocketmc-mark.png" alt="NeonPocketMC pocket mesh logo" width="140">
</p>

# NeonPocketMC-RCC6-Repeater

MeshCore repeater and room-server firmware for the **Heltec RadioCore RCC6-L62 / SX1262**, with optional native TFT, authenticated Wi-Fi dashboard, and MQTT observation.

> [!WARNING]
> Experimental RCC6-only firmware. Do not flash it to RC32, RC52, or another ESP32-C6/SX1262 board. Headless profiles deliberately power the TFT off. The full TFT room-server profile is explicitly experimental and must pass its delayed 32 KB heap-headroom gate after Wi-Fi, Web, and MQTT services start.

This project is based on MeshCore 1.17.0 and the production MQTT observer work from [`agessaman/MeshCore`](https://github.com/agessaman/MeshCore/tree/b744b42aabb454b277fe133214c7d93d23da484b). It adds the hardware mapping already proven by the NeonPocket RCC6 companion project.

## Choose the firmware profile

The established repeater build remains unchanged. Room Server is available as four explicit release environments assembled from shared common, TFT, and full-network bases:

| PlatformIO environment | Room service | 220x128 TFT | Wi-Fi dashboard | MQTT | Status |
|---|---:|---:|---:|---:|---|
| `heltec_rcc6_repeater_observer_mqtt` | No | Off | Yes | Yes | Existing repeater/observer |
| `heltec_rcc6_room_server_minimal_headless` | Yes | Off | No | No | Smallest room server |
| `heltec_rcc6_room_server_minimal_tft` | Yes | Yes | No | No | Room snapshot on device |
| `heltec_rcc6_room_server_full_headless` | Yes | Off | Yes | Yes | Recommended connected server |
| `heltec_rcc6_room_server_full_tft` | Yes | Yes | Yes | Yes | **Experimental**; delayed 32 KB heap gate |

“Minimal” means LoRa Room Server plus USB CLI only: it does not compile the Web/AP/MQTT feature stack. “Full” adds the proven AP/STA onboarding flow, authenticated LAN dashboard, and MQTT observer/ingester using the existing broker presets and credential machinery. MQTT remains one-way observation: broker traffic is never injected into the mesh.

The TFT variants use the RCC6 native 220x128 framebuffer driver with band-delta flushing and the animated NeonPocketMC startup mark. Their room snapshot shows active/registered clients, accepted posts, RF signal/age/errors, queue pressure, raw battery voltage, and heap. A provisional warning appears at or below 3.45 V and clears at or above 3.60 V; no automatic low-voltage shutdown is enabled before physical ADC calibration. Full TFT also shows Wi-Fi, MQTT slot health, free/minimum/max-allocation heap, and an `EXP` marker.

To list the same choices from the configurator without connecting hardware:

```text
python scripts/configure_rcc6.py --list-profiles
```

The USB configurator accepts only the two full Room Server profiles (and the existing repeater). It refuses minimal images because they intentionally have no Wi-Fi or MQTT settings to configure.

### Room Server safety and recovery

- Storage mounts fail closed. Existing nonblank SPIFFS data is never auto-formatted; only a completely erased partition is initialized.
- TFT builds stop on a visible error screen and serial fatal message if display, radio, storage, or the post-service heap gate fails. Headless builds emit the same radio/storage fatal state over USB serial.
- The full dashboard reuses the authenticated WebConfig API. Room activity is read-only: there are no HTTP endpoints for posting, deleting, changing clients, or injecting MQTT traffic into RF.
- A deliberate factory reset is USB-serial only: enter `factory-reset CONFIRM`. It erases MeshCore/MQTT files, clears stored Wi-Fi credentials from ESP32 NVS on full builds, waits for the reply to land, and reboots. This also erases the room identity and cannot be undone.

## What it does

- Runs a normal MeshCore repeater on the RCC6 SX1262 radio.
- Joins a local 2.4 GHz Wi-Fi network.
- Publishes observed mesh traffic to up to two MQTT brokers concurrently.
- Ships all 34 broker presets from the upstream observer firmware.
- Defaults slot 1 to `mqtt1.meshcore.ca` and slot 2 to `mqtt2.meshcore.ca`.
- Provides a guided USB setup tool plus a phone/desktop Web dashboard.
- Automatically starts the dashboard on the configured LAN after every boot.
- Defaults this repeater's own adverts to **3-byte path hashes** (mode `2`). MeshCore 1.17 still forwards incoming 1-, 2-, and 3-byte packets.
- Keeps six MQTT slot configurations on disk; the non-PSRAM RCC6 exposes three runtime slots and permits two active connections.
- Mounts MeshCore storage fail-closed. A mount failure will not silently format identity, contacts, channels, or settings.

## Easiest setup: keep USB connected

1. Attach a tuned LoRa antenna before powering or transmitting.
2. Flash the RC2 application image at `0x10000` and leave USB connected.
3. Download this repository (or the configurator ZIP from the [RC2 release](https://github.com/n30nex/NeonPocketMC-RCC6-Repeater/releases/tag/v1.0.0-rc.2)).
4. **Windows:** double-click `Configure-RCC6-Windows.cmd`.
5. **Linux:** open the downloaded folder in a terminal and run `sh configure-rcc6-linux.sh`.

The first run prepares a small private Python helper. The wizard then:

- finds the RCC6 and refuses to touch the wrong firmware;
- asks plain numbered questions for the node name and regional radio preset;
- sets frequency, bandwidth, spreading factor, coding rate, TX power, RX gain, and repeater mode;
- enforces 3-byte advert hashes;
- offers the Canadian MQTT defaults or every broker built into the firmware;
- stores the 2.4 GHz Wi-Fi and broker credentials without printing or logging them;
- asks for a new admin password, shows a final review, and changes nothing until confirmed;
- reads every saved value back, reboots, waits for Wi-Fi and the Web dashboard, then prints the exact IP and says when USB is safe to disconnect.

If anything fails, it stops and tells the user to keep USB connected.

## Web dashboard

On a brand-new or unconfigured unit, the RCC6 automatically creates `MeshCore-Setup-XXXX`. Join it and browse to [http://192.168.4.1/](http://192.168.4.1/) if the captive setup page does not open by itself.

Once Wi-Fi is configured, the authenticated dashboard starts automatically after every boot at:

```text
http://DEVICE-IP/
```

The USB wizard prints `DEVICE-IP`. Sign in with the device admin password.

### Find the device on your local network

Keep USB connected until the dashboard opens successfully.

1. The guided configurator prints a line such as `Web dashboard: http://192.168.0.39/` after the RCC6 joins Wi-Fi.
2. If that window was closed, open a 115200-baud USB serial terminal, press Enter, and run:

   ```text
   get wifi.status
   ```

   A connected device replies with its exact address, signal strength, and Wi-Fi uptime:

   ```text
   connected, IP: 192.168.0.39, RSSI: -25 dBm, uptime: 1m 4s
   ```

3. Open `http://DEVICE-IP/` from a phone or computer on the same local network. Use `http://`, not `https://`, and sign in with the admin password selected during setup.
4. If USB is unavailable, look in the router's DHCP/client list for the device that most recently joined.

The address is assigned by the router and can change. Create a DHCP reservation in the router if the repeater should always use the same address.

The phone/desktop dashboard includes:

- live health badges for LoRa, Wi-Fi, MQTT, memory, and firmware fault flags;
- packet RX/TX totals and per-minute rates, receive errors, flood/direct traffic, and duplicate counts;
- RSSI, SNR, noise floor, radio state, last-packet age, TX budget, airtime totals, and rolling TX/RX channel-load graphs;
- recent-neighbour count and a detailed recently-heard list with key prefix, age, advert age, and SNR;
- battery voltage, heap/free-block history, packet-pool headroom, CPU speed, and queue pressure;
- Wi-Fi RSSI/channel/IP plus per-broker connection state, publish successes/errors, and filters;
- rolling six-minute packet, RF, airtime, queue, memory, battery, and Wi-Fi graphs;
- guided radio/Wi-Fi/MQTT editing, all built-in broker choices, an advanced CLI, and safe reboot controls.

The setup AP is open, matching upstream behavior. Provision it at close range on a trusted network and change the default admin password immediately. The LAN dashboard uses plain HTTP with an application login, so operate it only on a trusted local network or through a trusted VPN; do not expose port 80 to the public Internet.

### Quick troubleshooting

| Symptom | What to do |
|---|---|
| `get wifi.status` says `disconnected` | Keep USB connected and rerun the supplied configurator. Confirm the SSID is a local 2.4 GHz network and re-enter its password. |
| The IP answers nowhere | Confirm the phone/computer is on the same LAN, enter `http://` explicitly, temporarily disconnect a VPN, and check that the router is not using wireless client isolation. |
| The IP changed | Run `get wifi.status` again or check the router's DHCP/client list, then add a DHCP reservation. |
| The admin password is forgotten | Rerun the USB configurator, or use a 115200-baud terminal and enter `password NEW-PASSWORD`, then `reboot`. |
| MQTT is not publishing | Run `get mqtt.status`; confirm Wi-Fi, broker presets, required broker credentials, and the configured IATA region. |
| Setup was never completed | Join `MeshCore-Setup-XXXX` and open [http://192.168.4.1/](http://192.168.4.1/), or use the supplied USB configurator. |

The release's configurator ZIP contains this complete guide as `SETUP_AND_HELP.md`, so it remains available offline after download.

## Manual setup and recovery

Prefer a browser? The upstream observer project provides the [agessaman Web Flasher](https://observer.gessaman.com/) and its [full setup manual](https://observer.gessaman.com/docs). This firmware is based on the [`agessaman/MeshCore` observer branch](https://github.com/agessaman/MeshCore/tree/observer-firmware). The generic [MeshCore USB configurator](https://config.meshcore.io/) remains useful for ordinary MeshCore settings, but it does not replace this firmware's MQTT-specific wizard.

Serial recovery/configuration is available at 115200 baud over USB. A complete minimal manual setup is:

```text
set name Hilltop Repeater
set radio 910.525,62.5,7,5
set tx 22
set radio.rxgain on
set repeat on
set path.hash.mode 2
set wifi.ssid Your 2.4 GHz SSID
set wifi.pwd Your WiFi password
set mqtt.iata YYZ
set mqtt1.preset meshcore-ca-1
set mqtt2.preset meshcore-ca-2
set mqtt.rx on
set mqtt.tx advert
password Choose-A-New-Password
get mqtt.status
get mqtt.presets
reboot
```

Any preset can replace either active slot. For example:

```text
set mqtt2.preset meshmapper
set mqtt1.preset none
set mqtt1.preset custom
set mqtt1.server wss://broker.example:443/mqtt
```

`set path.hash.mode 2` selects 3-byte path hashes for this repeater's own advert broadcasts. It does **not** limit forwarding: this MeshCore 1.17 repeater forwards packets using 1-, 2-, or 3-byte path hashes.

## Built-in broker presets

The RC2 build preserves all 34 presets from the pinned observer source. The two Canadian endpoints are the only RCC6-specific default change.

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
esptool --chip esp32c6 --port COMx write-flash 0x10000 NeonPocketMC-RCC6-Repeater-v1.0.0-rc.2-app.bin
```

Full recovery, rewriting the bootloader/partitions/application while leaving the later SPIFFS data partition untouched:

```powershell
esptool --chip esp32c6 --port COMx write-flash 0x0 NeonPocketMC-RCC6-Repeater-v1.0.0-rc.2-full-recovery-preserves-meshcore-settings.bin
```

Do not erase the whole flash if you want to retain identity and settings. Verify downloads with `SHA256SUMS.txt`.

## Source and scope

- MeshCore base: 1.17.0.
- MQTT observer base: `agessaman/MeshCore` `observer-firmware` at `b744b42aabb454b277fe133214c7d93d23da484b`.
- RCC6 hardware mapping provenance: the separately tested `NeonPocketMC-RCC6` companion project.
- Release targets: the established `heltec_rcc6_repeater_observer_mqtt` plus all four explicit Room Server profiles listed above.
- License: MIT; dependency notices and licenses remain in the source tree.

All builds remain prerelease firmware. The observer/repeater keeps its existing behavior; the new Room Server profiles share the same guided USB configuration, 3-byte advert-hash default, full-profile LAN dashboard, and optional MQTT observation stack.
