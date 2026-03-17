# Water Bucket Controller (ESP32) — Board V2

ESP-IDF firmware: six pumps via 74HC238 decoder (one at a time), three water-level inputs, SSD1306 OLED (test UI), rotary encoder (test logging), MQTT for Home Assistant. When all level sensors read dry (no water), pumps are disabled.

## Hardware (V2)

| Role | Pins | Logic |
|------|------|--------|
| Decoder enable | GPIO 17 | HIGH = decoder on; LOW = all decoder outputs inactive |
| Decoder A / B / C | GPIO 18 / 16 / 19 | Binary select for pump index 0–5 (A = LSB) |
| Pumps | — | Only one pump on at a time; index 0..5 selects 74HC238 output |
| Level high | GPIO 13 | HIGH = dry, LOW = water |
| Level medium | GPIO 14 | HIGH = dry, LOW = water |
| Level low | GPIO 25 | HIGH = dry, LOW = water |
| Safety | — | All three levels HIGH (dry) → pumps disabled |
| OLED SSD1306 I2C | SDA 22, SCL 32 | Address 0x3C; “Hello World” at boot |
| Rotary encoder | SW 27, A 26, B 33 | Internal pull-ups; CW/CCW and press logged as `wb_ui` |

Flash: use **4MB** or larger when using the two-slot OTA partition table (`sdkconfig.defaults`).

## Source layout

`main/`: `gpio.c` (decoder + level pins), `level.c`, `pump.c`, `mqtt.c`, `wifi.c`, `log_tcp.c`, `ota.c`, `lcd.c`, `rotary_encoder.c`, `ui_test.c`, `priv.h`, `main.cpp`.

## Build and flash

**Prerequisites:** ESP-IDF v5.x or v6.x, `idf.py` in PATH.

1. Set target: `idf.py set-target esp32` (or `make set-target`)
2. Configure WiFi and MQTT: copy `main/wb_config.h.example` to `main/wb_config.h` and set `WB_WIFI_SSID`, `WB_WIFI_PASSWORD`, `WB_MQTT_BROKER_URI`; optionally `WB_MQTT_USER`, `WB_MQTT_PASSWORD`. Set `WB_LOG_TCP_PORT` (default 8080) or 0 to disable log-over-WiFi.
3. Build: `make` or `idf.py build`
4. Flash: `make flash` (optionally `make flash PORT=/dev/cu.usbserial-xxx`). Monitor: `make monitor`. Build + flash + monitor: `make watch`.

**Find the serial port:** same as usual (`ls /dev/cu.*` on macOS, etc.).

## OTA (Over-The-Air) updates

See [ESP-IDF OTA](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html). Options are in `sdkconfig.defaults`. First-time OTA layout: `idf.py fullclean` then build and flash once. Trigger via MQTT `water_bucket/cmd/ota` with firmware URL. Rollback uses level GPIO reads (13/14/25) after an OTA boot.

## Monitor logs over WiFi

After WiFi has an IP, logs mirror to TCP port `WB_LOG_TCP_PORT` (e.g. `nc <IP> 8080`). Tag `wb`: levels, pumps, MQTT. Tag `wb_ui`: OLED init, encoder.

## Testing

- **Serial:** `idf.py -p PORT monitor`; filter `wb` / `wb_ui`.
- **MQTT:** Subscribe `water_bucket/state/#`. Publish `water_bucket/cmd/pump` with `0`–`5` or `off`.
- **Hardware:** All levels dry → pump commands rejected. Any level wet → pumps 0–5 one at a time; state on `water_bucket/state/pump`.

## Home Assistant

Broker on 1883; discovery publishes six pump switches and three level binary sensors. `water_bucket/state/level_1` = high level, `level_2` = medium, `level_3` = low (payload `1` = dry, `0` = water).

Pump switches: command topic `water_bucket/cmd/pump`, payload_on `0`–`5`, payload_off `off`, state topic `water_bucket/state/pump` with per-pump value_template.

### Topic list

| Topic | Payload | Direction |
|-------|---------|-----------|
| water_bucket/status | online / offline | ESP32 → HA |
| water_bucket/cmd/ota | firmware URL | HA → ESP32 |
| water_bucket/state/level_1..3 | 0 or 1 | ESP32 → HA |
| water_bucket/state/pump | 0–5 or off | ESP32 → HA |
| water_bucket/cmd/pump | 0–5 or off | HA → ESP32 |

For manual YAML, duplicate the four-pump `switch` pattern through pump 5 and merge under one `mqtt:` key.
