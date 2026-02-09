# Water Bucket Controller (ESP32)

ESP-IDF firmware for a water bucket controller: four pumps (one at a time), three water-level inputs, and MQTT for Home Assistant. When all level sensors read dry (no water), pumps are disabled.

## Hardware

| Role | Pins | Logic |
|------|------|--------|
| Pumps (outputs) | GPIO 16, 17, 18, 19 | HIGH = pump on; only one pump on at a time |
| Level sensors (inputs) | GPIO 32, 33, 35 | HIGH = no water at level, LOW = water at level |
| Safety | — | All three sensors HIGH → pumps disabled |

GPIO 35 is input-only; the circuit must drive it high when dry.

## Source layout

`main/` contains: `gpio.c` (shared GPIO init and level/pump pin access), `level.c` (level sensing and pumps-disabled logic), `pump.c` (pump selection and state publish), `mqtt.c` (MQTT client, publish/subscribe), `wifi.c` (STA init, block until IP or timeout), `log_tcp.c` (TCP log server for monitoring over WiFi), `priv.h` (internal API and shared state), `main.cpp` (app_main and init order).

## Build and flash

**Prerequisites:** ESP-IDF v5.x or v6.x, `idf.py` in PATH.

1. Set target: `idf.py set-target esp32`
2. Configure WiFi and MQTT: copy `main/wb_config.h.example` to `main/wb_config.h` and set `WB_WIFI_SSID`, `WB_WIFI_PASSWORD`, `WB_MQTT_BROKER_URI`; optionally `WB_MQTT_USER`, `WB_MQTT_PASSWORD`. Set `WB_LOG_TCP_PORT` (default 8080) or 0 to disable log-over-WiFi.
3. Build: `idf.py build`
4. Flash and monitor: `idf.py -p PORT flash monitor` — replace `PORT` with your serial port. If you omit `-p PORT`, `idf.py` will prompt or auto-detect.

**Find the serial port:**

| OS | Command |
|----|--------|
| **Windows** | PowerShell: `[System.IO.Ports.SerialPort]::getportnames()` or Device Manager → Ports (COM & LPT) |
| **Linux** | `ls /dev/ttyUSB* /dev/ttyACM*` |
| **macOS** | `ls /dev/cu.*` |

Flashing is over USB/serial only.

## Monitor logs over WiFi

After WiFi has an IP, all `ESP_LOG*` output is mirrored to a TCP server (in addition to USB serial). One client at a time can connect to view a live stream of log lines.

- **Port:** configurable in `wb_config.h` as `WB_LOG_TCP_PORT` (default **8080**). Set to **0** to disable the TCP log server.
- **Connect:** from a PC on the same network run `nc <ESP32_IP> 8080` (macOS/Linux). Replace `<ESP32_IP>` with the device’s IP (from your router/DHCP or from a one-time serial monitor session).
- **Example:** `nc 10.0.0.154 8080`

## Home Assistant setup

**MQTT broker:** Install the Mosquitto add-on (Settings → Add-ons → Add-on Store). Enable "Start on boot" and "Watchdog". Note broker address (usually your HA host) and port (default 1883). If the broker uses auth, create a user in HA and set WB_MQTT_USER / WB_MQTT_PASSWORD in `wb_config.h`.

**MQTT integration:** In HA go to Settings → Integrations → Add → MQTT and configure the broker. Set `WB_MQTT_BROKER_URI` in `wb_config.h` to the same broker (e.g. `mqtt://10.0.0.126:1883`).

**Entities (manual):** After the device is connected and publishing:

- **Binary sensors:** MQTT binary sensors for `water_bucket/state/level_1`, `level_2`, `level_3` — payload `1` = no water, `0` = water at that level.
- **Pump control:** Command topic `water_bucket/cmd/pump`. Payloads: `0`, `1`, `2`, `3` (pump index) or `off`. State topic: `water_bucket/state/pump` (payload `0`–`3` or `off`; `off` = all pumps off, including when all levels are dry).

**Topic list (for automations):**

| Topic | Payload | Direction |
|-------|---------|-----------|
| water_bucket/state/level_1 | 0 or 1 | ESP32 → HA |
| water_bucket/state/level_2 | 0 or 1 | ESP32 → HA |
| water_bucket/state/level_3 | 0 or 1 | ESP32 → HA |
| water_bucket/state/pump | 0, 1, 2, 3, or off | ESP32 → HA |
| water_bucket/cmd/pump | 0, 1, 2, 3, or off | HA → ESP32 |

## Testing (outside ESPHome)

**Serial monitor:** Run `idf.py -p PORT monitor` after flashing. Watch logs for WiFi/MQTT connect, level readings, and pump command handling. Use `idf.py monitor --print-filter wb:info` to filter by tag if needed.

**Logs over WiFi:** Once the ESP32 has an IP, run `nc <ESP32_IP> 8080` (see **Monitor logs over WiFi** above) to stream logs without USB.

**MQTT without Home Assistant:** Use any MQTT client (e.g. `mosquitto_pub` / `mosquitto_sub`, or MQTT Explorer). Subscribe to `water_bucket/state/#` to see level_1/2/3 and pump. Publish to `water_bucket/cmd/pump` with payload `0`, `1`, `2`, `3`, or `off` to drive pumps. Ensure `WB_MQTT_BROKER_URI` in `wb_config.h` matches the machine running the broker.

**Hardware checks:**

1. With all level sensors dry (or pins 32/33/35 high): confirm pump state is `off`; send a pump command via MQTT and confirm it is rejected (no pump turns on).
2. Ground one level pin (or simulate water): send `water_bucket/cmd/pump` with `0` then `1` and confirm only one pump is on at a time and `water_bucket/state/pump` reflects the active pump.
3. Send `off` and confirm all pumps off.
