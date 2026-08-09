#!/usr/bin/env python3
"""Guided USB configurator for NeonPocketMC RCC6 full network profiles."""

from __future__ import annotations

import argparse
import getpass
import json
import re
import sys
import time
import urllib.error
import urllib.request


USB_VID = 0x303A
USB_PID = 0x1001
BAUD = 115200

BUILD_PROFILES = (
    ("heltec_rcc6_room_server_minimal_headless", "Room core only; no TFT, Wi-Fi, Web UI, or MQTT"),
    ("heltec_rcc6_room_server_minimal_tft", "Room core plus native 220x128 TFT; no Wi-Fi or MQTT"),
    ("heltec_rcc6_room_server_full_headless", "Room core plus authenticated AP/STA Web UI and MQTT"),
    ("heltec_rcc6_room_server_full_tft", "Full profile plus TFT and 32 KB heap gate (experimental)"),
)

RADIO_PRESETS = [
    ("USA/Canada (recommended)", "910.525", "62.5", 7, 5),
    ("Australia", "915.800", "250", 10, 5),
    ("Australia (narrow)", "916.575", "62.5", 7, 8),
    ("Australia (mid)", "915.075", "125", 9, 5),
    ("Australia: SA/WA", "923.125", "62.5", 8, 8),
    ("Australia: QLD", "923.125", "62.5", 8, 5),
    ("Brazil", "923.125", "62.5", 8, 8),
    ("EU/UK (narrow)", "869.618", "62.5", 8, 8),
    ("EU/UK (legacy wide)", "869.525", "250", 11, 5),
    ("Czech Republic (narrow)", "869.432", "62.5", 7, 5),
    ("EU 433 MHz (long range)", "433.650", "250", 11, 5),
    ("EU 433 MHz (narrow)", "433.650", "62.5", 8, 8),
    ("Netherlands", "869.618", "62.5", 7, 5),
    ("New Zealand", "917.375", "250", 11, 5),
    ("New Zealand (narrow)", "917.375", "62.5", 7, 5),
    ("Portugal 433", "433.375", "62.5", 9, 6),
    ("Portugal 868", "869.618", "62.5", 7, 6),
    ("Switzerland", "869.618", "62.5", 8, 8),
    ("Vietnam (narrow)", "920.250", "62.5", 8, 5),
    ("Vietnam (legacy wide)", "920.250", "250", 11, 5),
]

FALLBACK_MQTT_PRESETS = [
    "analyzer-us", "analyzer-eu", "nz-analyzer", "meshmapper", "meshrank",
    "waev", "meshomatic", "cascadiamesh", "tennmesh", "nashmesh", "ctmesh",
    "chimesh", "meshat.se", "eastidahomesh", "coloradomesh",
    "dutchmeshcore-1", "dutchmeshcore-2", "meshcore-ca-1", "meshcore-ca-2",
    "meshcore-fi", "okimesh-1", "okimesh-2", "inwmesh", "bostonmesh",
    "rflab", "ipnt.uk", "flmesh", "corecomms", "meshtexas", "mesh-chaun14",
    "wcmesh", "atvirastinklas", "gomesh", "idahomesh", "none",
]

FORBIDDEN_NAME_CHARS = set("[]/\\: ,?*") - {" "}


class SetupError(RuntimeError):
    pass


def clean_reply(reply: str) -> str:
    reply = reply.strip()
    return reply[1:].strip() if reply.startswith(">") else reply


def is_error(reply: str) -> bool:
    value = reply.strip().lower()
    return value.startswith(("error", "err", "unknown", "??"))


class DeviceCLI:
    def __init__(self, port: str):
        try:
            import serial
        except ImportError as exc:
            raise SetupError("PySerial is missing. Start the configurator with the supplied launcher.") from exc

        self.port = port
        try:
            # Configure control lines before opening so the wizard does not
            # accidentally put the ESP32-C6 into its ROM download mode.
            self.serial = serial.Serial()
            self.serial.port = port
            self.serial.baudrate = BAUD
            self.serial.timeout = 0.08
            self.serial.write_timeout = 2
            self.serial.dtr = False
            self.serial.rts = False
            self.serial.open()
        except Exception as exc:
            hint = " On Linux, add your user to the dialout group and sign in again." if sys.platform != "win32" else ""
            raise SetupError(f"Could not open {port}: {exc}.{hint}") from exc
        time.sleep(1.2)
        self.serial.reset_input_buffer()

    def close(self) -> None:
        if getattr(self, "serial", None) and self.serial.is_open:
            self.serial.close()

    def command(self, command: str, *, timeout: float = 4.0, quiet: bool = False,
                secret: bool = False) -> str:
        label = command.split(" ", 1)[0] + " ********" if secret else command
        if not quiet:
            print(f"  {label:<48}", end="", flush=True)
        self.serial.reset_input_buffer()
        self.serial.write((command + "\r").encode("utf-8"))
        self.serial.flush()

        deadline = time.monotonic() + timeout
        data = bytearray()
        reply = None
        while time.monotonic() < deadline:
            chunk = self.serial.read(max(1, self.serial.in_waiting))
            if chunk:
                data.extend(chunk)
                text = data.decode("utf-8", errors="replace")
                matches = re.findall(r"(?:^|[\r\n])\s*->\s*([^\r\n]*)", text)
                if matches:
                    reply = matches[-1].strip()
                    if reply:
                        break
            else:
                time.sleep(0.03)

        if reply is None:
            if not quiet:
                print("NO REPLY")
            raise SetupError(f"The device did not answer '{label}'. Keep USB connected and run the wizard again.")
        if is_error(reply):
            if not quiet:
                print("FAILED")
            raise SetupError(f"The device rejected '{label}': {reply}")
        if not quiet:
            print("OK")
        return reply

    def value(self, key: str) -> str:
        return clean_reply(self.command(f"get {key}", quiet=True))

    def reboot(self) -> None:
        print("  rebooting device", flush=True)
        self.serial.reset_input_buffer()
        self.serial.write(b"reboot\r")
        self.serial.flush()
        time.sleep(0.8)
        self.close()


def list_ports():
    try:
        from serial.tools import list_ports as serial_list_ports
    except ImportError as exc:
        raise SetupError("PySerial is missing. Start the configurator with the supplied launcher.") from exc
    return sorted(serial_list_ports.comports(), key=lambda p: (p.device.lower()))


def choose_port(requested: str | None) -> str:
    if requested:
        return requested
    ports = list_ports()
    if not ports:
        raise SetupError("No serial devices found. Connect the RCC6 with a USB data cable and try again.")

    preferred = [p for p in ports if p.vid == USB_VID and p.pid == USB_PID]
    if len(preferred) == 1:
        p = preferred[0]
        print(f"Found RCC6 candidate: {p.device} ({p.description})")
        return p.device

    print("\nConnected serial devices:")
    for index, port in enumerate(ports, 1):
        marker = "  <-- likely RCC6" if port in preferred else ""
        print(f"  {index:2}. {port.device:<14} {port.description}{marker}")
    return ports[ask_number("Choose the RCC6 USB port", 1, len(ports)) - 1].device


def verify_device(cli: DeviceCLI) -> tuple[str, str, str, str]:
    version = cli.command("ver", quiet=True)
    board = cli.command("board", quiet=True)
    role = cli.value("role")
    version_l = version.lower()
    role_l = role.lower()
    supported = (
        ("rcc6-mqtt" in version_l and "repeater" in role_l) or
        ("rcc6-room" in version_l and "room_server" in role_l)
    )
    if not supported or "rcc6" not in board.lower():
        raise SetupError(
            f"Refusing to configure this device. It reported '{version}', '{board}', role '{role}'."
        )
    profile = "repeater-observer"
    if "room_server" in role_l:
        profile = cli.value("room.profile")
        if profile.startswith("minimal-"):
            raise SetupError(
                f"This device runs '{profile}', which intentionally omits Wi-Fi/MQTT. "
                "Flash a full-headless or full-tft build before using this wizard."
            )
    print(f"Verified: {board} / {version} / {role} / {profile}")
    return version, board, role, profile


def ask_text(label: str, *, default: str = "", maximum: int | None = None,
             validator=None, secret: bool = False, allow_blank: bool = False) -> str:
    while True:
        suffix = f" [{default}]" if default and not secret else ""
        prompt = f"{label}{suffix}: "
        value = getpass.getpass(prompt) if secret else input(prompt).strip()
        if not value and default:
            value = default
        if not value and allow_blank:
            return ""
        if not value:
            print("  A value is required.")
            continue
        if "\r" in value or "\n" in value:
            print("  Line breaks are not allowed.")
            continue
        if maximum is not None and len(value.encode("utf-8")) > maximum:
            print(f"  Maximum length is {maximum} bytes.")
            continue
        try:
            valid = validator is None or validator(value)
        except (TypeError, ValueError, OverflowError):
            valid = False
        if not valid:
            print("  That value is not valid.")
            continue
        return value


def ask_confirmed_password(label: str) -> str:
    while True:
        value = ask_text(f"New {label} (8-15 characters)", maximum=15,
                         validator=lambda text: len(text) >= 8, secret=True)
        if value == getpass.getpass(f"Confirm {label}: "):
            return value
        print("  Passwords did not match. Try again.")


def ask_number(label: str, minimum: int, maximum: int, default: int | None = None) -> int:
    while True:
        suffix = f" [{default}]" if default is not None else ""
        raw = input(f"{label}{suffix}: ").strip()
        if not raw and default is not None:
            return default
        try:
            value = int(raw)
        except ValueError:
            value = minimum - 1
        if minimum <= value <= maximum:
            return value
        print(f"  Enter a number from {minimum} to {maximum}.")


def ask_yes_no(label: str, default: bool = True) -> bool:
    choice = "Y/n" if default else "y/N"
    while True:
        raw = input(f"{label} [{choice}]: ").strip().lower()
        if not raw:
            return default
        if raw in ("y", "yes"):
            return True
        if raw in ("n", "no"):
            return False
        print("  Enter Y or N.")


def valid_name(value: str) -> bool:
    return 1 <= len(value.encode("utf-8")) <= 31 and not any(c in FORBIDDEN_NAME_CHARS for c in value)


def valid_iata(value: str) -> bool:
    return bool(re.fullmatch(r"[A-Za-z0-9]{3}", value))


def radio_key(values) -> tuple[float, float, int, int]:
    return float(values[0]), float(values[1]), int(values[2]), int(values[3])


def choose_radio(current: str):
    print("\nRadio region")
    print("Your other MeshCore radios must use the same settings.")
    for index, (name, freq, bw, sf, cr) in enumerate(RADIO_PRESETS, 1):
        print(f"  {index:2}. {name:<30} {freq} MHz / BW {bw} / SF {sf} / CR {cr}")
    custom_index = len(RADIO_PRESETS) + 1
    print(f"  {custom_index:2}. Custom (advanced)")

    default = None
    try:
        current_key = radio_key(current.split(","))
        for index, preset in enumerate(RADIO_PRESETS, 1):
            if radio_key(preset[1:]) == current_key:
                default = index
                break
    except (ValueError, IndexError):
        pass

    choice = ask_number("Choose your local mesh preset", 1, custom_index, default)
    if choice <= len(RADIO_PRESETS):
        name, freq, bw, sf, cr = RADIO_PRESETS[choice - 1]
        return name, freq, bw, sf, cr

    freq = ask_text("Frequency in MHz", validator=lambda x: 150 <= float(x) <= 2500)
    bw = ask_text("Bandwidth in kHz", validator=lambda x: 7 <= float(x) <= 500)
    sf = ask_number("Spreading factor", 5, 12)
    cr = ask_number("Coding rate", 5, 8)
    return "Custom", freq, bw, sf, cr


def fetch_mqtt_presets(cli: DeviceCLI) -> list[str]:
    names = []
    start = 0
    try:
        while True:
            command = "get mqtt.presets" if start == 0 else f"get mqtt.presets {start}"
            reply = clean_reply(cli.command(command, quiet=True))
            page, marker, tail = reply.partition("... next:")
            names.extend(name.strip() for name in page.rstrip(" ,").split(",") if name.strip())
            if not marker:
                break
            start = int(tail.strip())
        return [name for name in names if name != "custom"]
    except (SetupError, ValueError):
        return FALLBACK_MQTT_PRESETS.copy()


def choose_broker(label: str, presets: list[str], default: str) -> str:
    print(f"\n{label}")
    for index, name in enumerate(presets, 1):
        print(f"  {index:2}. {name}")
    default_index = presets.index(default) + 1 if default in presets else None
    return presets[ask_number("Choose a preset", 1, len(presets), default_index) - 1]


def gather_broker_credentials(slot: int, preset: str) -> dict[str, str]:
    result = {}
    if preset == "meshrank":
        result["token"] = ask_text(f"MQTT slot {slot} token", maximum=47, secret=True)
    elif preset == "inwmesh":
        result["username"] = ask_text(f"MQTT slot {slot} username", maximum=31)
        result["password"] = ask_text(f"MQTT slot {slot} password", maximum=63, secret=True)
    elif preset == "mesh-chaun14":
        result["password"] = ask_text(f"MQTT slot {slot} password", maximum=63, secret=True)
    return result


def read_current(cli: DeviceCLI) -> dict[str, str]:
    keys = ("name", "radio", "tx", "repeat", "wifi.ssid", "mqtt.iata",
            "mqtt1.preset", "mqtt2.preset", "path.hash.mode")
    current = {}
    for key in keys:
        try:
            current[key] = cli.value(key)
        except SetupError:
            current[key] = ""
    return current


def build_commands(config: dict) -> list[tuple[str, bool]]:
    commands = [
        (f"set name {config['name']}", False),
        (f"set radio {config['freq']},{config['bw']},{config['sf']},{config['cr']}", False),
        (f"set tx {config['tx']}", False),
        ("set radio.rxgain on", False),
        (f"set repeat {'on' if config['repeat'] else 'off'}", False),
        ("set path.hash.mode 2", False),
        (f"set mqtt.origin {config['name']}", False),
        (f"set mqtt.iata {config['iata']}", False),
        ("set mqtt1.preset none", False),
        ("set mqtt2.preset none", False),
        (f"set mqtt1.preset {config['mqtt1']}", False),
        (f"set mqtt2.preset {config['mqtt2']}", False),
        ("set mqtt.rx on", False),
        ("set mqtt.tx advert", False),
        (f"set wifi.ssid {config['ssid']}", False),
    ]
    if config["wifi_password"] is not None:
        commands.append((f"set wifi.pwd {config['wifi_password']}", True))
    for slot in (1, 2):
        for field, value in config[f"mqtt{slot}_credentials"].items():
            commands.append((f"set mqtt{slot}.{field} {value}", True))
    commands.append((f"password {config['admin_password']}", True))
    if config.get("room_server"):
        commands.append((f"set guest.password {config['guest_password']}", True))
    return commands


def gather_config(cli: DeviceCLI, current: dict[str, str], *, room_server: bool = False) -> dict:
    print("\nStep 1 of 4 - Node and Wi-Fi")
    name = ask_text("Node name", default=current["name"], maximum=31, validator=valid_name)
    ssid = ask_text("2.4 GHz Wi-Fi network name", default=current["wifi.ssid"], maximum=31)
    wifi_password = getpass.getpass("Wi-Fi password (hidden; blank keeps the saved password): ")
    if len(wifi_password) > 63:
        raise SetupError("Wi-Fi password is longer than 63 characters.")
    if not wifi_password:
        if current["wifi.ssid"] and ssid == current["wifi.ssid"]:
            wifi_password = None
        elif not ask_yes_no("This will configure an open Wi-Fi network. Continue?", False):
            raise SetupError("Setup cancelled before making changes.")

    print("\nStep 2 of 4 - Radio")
    radio_name, freq, bw, sf, cr = choose_radio(current["radio"])
    try:
        current_tx = int(current["tx"])
    except ValueError:
        current_tx = 22
    tx = ask_number("TX power in dBm (check your local legal limit)", -9, 22, current_tx)
    repeat_label = ("Forward mesh traffic in addition to hosting the room" if room_server
                    else "Forward mesh traffic (repeater mode)")
    repeat = ask_yes_no(repeat_label, current["repeat"].lower() != "off")

    print("\nStep 3 of 4 - MQTT")
    iata = ask_text("Nearest 3-character airport/IATA code", default=current["mqtt.iata"].upper(),
                    maximum=3, validator=valid_iata).upper()
    presets = fetch_mqtt_presets(cli)
    print("\n  1. Use Canadian defaults: mqtt1.meshcore.ca + mqtt2.meshcore.ca")
    print("  2. Choose from every built-in broker")
    print("  3. Disable MQTT publishing")
    default_mode = 1 if (current["mqtt1.preset"], current["mqtt2.preset"]) == (
        "meshcore-ca-1", "meshcore-ca-2") else 2
    mode = ask_number("MQTT setup", 1, 3, default_mode)
    if mode == 1:
        mqtt1, mqtt2 = "meshcore-ca-1", "meshcore-ca-2"
    elif mode == 3:
        mqtt1 = mqtt2 = "none"
    else:
        mqtt1 = choose_broker("MQTT server 1", presets, current["mqtt1.preset"])
        mqtt2 = choose_broker("MQTT server 2", presets, current["mqtt2.preset"])
        if mqtt1 != "none" and mqtt1 == mqtt2:
            raise SetupError("The same broker cannot be used in both slots. Run the wizard again and choose two different servers.")

    credentials1 = gather_broker_credentials(1, mqtt1)
    credentials2 = gather_broker_credentials(2, mqtt2)

    print("\nStep 4 of 4 - Security")
    admin_password = ask_confirmed_password("device admin password")
    guest_password = ask_confirmed_password("room guest password") if room_server else None

    return {
        "name": name, "ssid": ssid, "wifi_password": wifi_password,
        "radio_name": radio_name, "freq": freq, "bw": bw, "sf": sf, "cr": cr,
        "tx": tx, "repeat": repeat, "iata": iata, "mqtt1": mqtt1, "mqtt2": mqtt2,
        "mqtt1_credentials": credentials1, "mqtt2_credentials": credentials2,
        "admin_password": admin_password, "guest_password": guest_password,
        "room_server": room_server,
    }


def show_summary(port: str, config: dict) -> None:
    print("\nReview - nothing has been changed yet")
    print(f"  USB port:       {port}")
    print(f"  Node name:      {config['name']}")
    print(f"  Radio preset:   {config['radio_name']}")
    print(f"  Radio values:   {config['freq']} MHz, BW {config['bw']}, SF {config['sf']}, CR {config['cr']}")
    print(f"  TX power:       {config['tx']} dBm")
    mode = "room + repeater" if config.get("room_server") and config["repeat"] else (
        "room server only" if config.get("room_server") else
        ("on" if config["repeat"] else "off (observer only)"))
    print(f"  Mesh role:      {mode}")
    print("  Advert hashes:  3 bytes")
    print(f"  MQTT region:    {config['iata']}")
    print(f"  MQTT servers:   {config['mqtt1']} / {config['mqtt2']}")
    print(f"  Wi-Fi network:  {config['ssid']}")
    print("  Passwords:      hidden and never saved by this script")


def verify_saved(cli: DeviceCLI, config: dict) -> None:
    expected = {
        "name": config["name"],
        "radio": f"{config['freq']},{config['bw']},{config['sf']},{config['cr']}",
        "tx": str(config["tx"]),
        "repeat": "on" if config["repeat"] else "off",
        "path.hash.mode": "2",
        "wifi.ssid": config["ssid"],
        "mqtt.iata": config["iata"],
        "mqtt1.preset": config["mqtt1"],
        "mqtt2.preset": config["mqtt2"],
        "mqtt.rx": "on",
        "mqtt.tx": "advert",
    }
    for key, wanted in expected.items():
        actual = cli.value(key)
        if key == "radio":
            try:
                if radio_key(actual.split(",")) == radio_key(wanted.split(",")):
                    continue
            except (ValueError, IndexError):
                pass
        elif actual.lower() == wanted.lower():
            continue
        raise SetupError(f"Verification failed for {key}: device has '{actual}', expected '{wanted}'.")
    if config.get("room_server") and cli.value("guest.password") != config["guest_password"]:
        raise SetupError("Verification failed for the room guest password.")


def wait_for_reboot(port: str, timeout: int = 35) -> DeviceCLI:
    print("Waiting for the RCC6 to return", end="", flush=True)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        print(".", end="", flush=True)
        try:
            cli = DeviceCLI(port)
            verify_device(cli)
            print()
            return cli
        except SetupError:
            time.sleep(1)
    print()
    raise SetupError("The RCC6 did not return after reboot. Keep USB connected and press Reset once.")


def wait_for_wifi(cli: DeviceCLI, timeout: int = 50) -> tuple[str, str]:
    print("Waiting for 2.4 GHz Wi-Fi", end="", flush=True)
    deadline = time.monotonic() + timeout
    last = "no reply"
    while time.monotonic() < deadline:
        try:
            last = cli.value("wifi.status")
            match = re.search(r"\bIP:\s*([0-9]+(?:\.[0-9]+){3})", last)
            if match:
                print()
                return match.group(1), last
        except SetupError as exc:
            last = str(exc)
        print(".", end="", flush=True)
        time.sleep(2)
    print()
    raise SetupError(f"Wi-Fi did not connect. Last status: {last}. Keep USB connected and run the wizard again.")


def wait_for_webui(ip: str, timeout: int = 20) -> None:
    url = f"http://{ip}/api/status"
    print("Waiting for the Web dashboard", end="", flush=True)
    deadline = time.monotonic() + timeout
    last = "no reply"
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(url, timeout=2) as response:
                payload = json.loads(response.read().decode("utf-8"))
            if payload.get("board") and str(payload.get("role", "")).lower() in (
                    "repeater", "room_server"):
                print()
                return
            last = "unexpected device response"
        except (OSError, ValueError, urllib.error.URLError) as exc:
            last = str(exc)
        print(".", end="", flush=True)
        time.sleep(1)
    print()
    raise SetupError(
        f"The device joined Wi-Fi at {ip}, but its Web dashboard did not answer ({last}). "
        "Keep USB connected and run the wizard again."
    )


def self_test() -> None:
    assert valid_name("Hilltop Repeater")
    assert not valid_name("bad/name")
    assert valid_iata("YYZ") and not valid_iata("Toronto")
    assert len(RADIO_PRESETS) == 20
    assert radio_key(RADIO_PRESETS[0][1:]) == (910.525, 62.5, 7, 5)
    assert len(BUILD_PROFILES) == 4
    assert BUILD_PROFILES[-1][0] == "heltec_rcc6_room_server_full_tft"
    sample = {
        "name": "Test", "freq": "910.525", "bw": "62.5", "sf": 7, "cr": 5,
        "tx": 22, "repeat": True, "iata": "YYZ", "mqtt1": "meshcore-ca-1",
        "mqtt2": "meshcore-ca-2", "ssid": "Example", "wifi_password": "secret",
        "mqtt1_credentials": {}, "mqtt2_credentials": {}, "admin_password": "adminpass",
        "guest_password": "guestpass", "room_server": True,
    }
    commands = build_commands(sample)
    assert ("set path.hash.mode 2", False) in commands
    assert ("set radio 910.525,62.5,7,5", False) in commands
    assert ("set wifi.pwd secret", True) in commands
    assert ("set guest.password guestpass", True) in commands
    observer = dict(sample, room_server=False, guest_password=None)
    assert not any(command.startswith("set guest.password ") for command, _ in build_commands(observer))
    print("Configurator self-test passed")


def print_profiles() -> None:
    print("NeonPocketMC RCC6 room-server firmware profiles:\n")
    for name, description in BUILD_PROFILES:
        print(f"  {name}\n    {description}")
    print("\nThis USB wizard configures the two full profiles. Minimal profiles intentionally have no Wi-Fi/MQTT setup.")


def main() -> int:
    parser = argparse.ArgumentParser(description="Configure a NeonPocketMC RCC6 full network profile over USB")
    parser.add_argument("--port", help="serial port, for example COM21 or /dev/ttyACM0")
    parser.add_argument("--list-profiles", action="store_true", help="show the four room-server build profiles and exit")
    parser.add_argument("--self-test", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if args.list_profiles:
        print_profiles()
        return 0

    print("\nNeonPocketMC RCC6 Network Setup")
    print("================================")
    print("Keep USB and a tuned LoRa antenna connected until this wizard says setup is complete.")
    print("This wizard is for full headless/full TFT profiles; minimal profiles omit Wi-Fi and MQTT.")

    cli = None
    try:
        port = choose_port(args.port)
        cli = DeviceCLI(port)
        _, _, role, _ = verify_device(cli)
        current = read_current(cli)
        config = gather_config(cli, current, room_server=role.lower() == "room_server")
        show_summary(port, config)
        if not ask_yes_no("Apply these settings and reboot", False):
            print("No changes were made.")
            return 0

        print("\nApplying settings")
        for command, secret in build_commands(config):
            cli.command(command, secret=secret)
        verify_saved(cli, config)
        print("  saved values verified")

        cli.reboot()
        cli = wait_for_reboot(port)
        ip, wifi_status = wait_for_wifi(cli)
        wait_for_webui(ip)
        mqtt_status = cli.value("mqtt.status")

        print("\nSETUP COMPLETE")
        print(f"  Device IP:      {ip}")
        print(f"  Wi-Fi:          {wifi_status}")
        print(f"  MQTT:           {mqtt_status}")
        print(f"  Web dashboard:  http://{ip}/")
        print("  Dashboard login: your new device admin password")
        print("\nYou may now disconnect USB and deploy the node.")
        return 0
    except (KeyboardInterrupt, EOFError):
        print("\nSetup cancelled. Keep USB connected if configuration was in progress.")
        return 130
    except SetupError as exc:
        print(f"\nSETUP STOPPED: {exc}")
        print("Do not deploy or disconnect USB until the problem is corrected.")
        return 2
    finally:
        if cli:
            cli.close()


if __name__ == "__main__":
    raise SystemExit(main())
