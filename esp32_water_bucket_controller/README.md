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

## Testing

**Serial monitor:** Run `idf.py -p PORT monitor` after flashing. Watch logs for WiFi/MQTT connect, level readings, and pump command handling. Use `idf.py monitor --print-filter wb:info` to filter by tag if needed.

**Logs over WiFi:** Once the ESP32 has an IP, run `nc <ESP32_IP> 8080` (see **Monitor logs over WiFi** above) to stream logs without USB.

**MQTT without Home Assistant:** Use any MQTT client (e.g. `mosquitto_pub` / `mosquitto_sub`, or MQTT Explorer). Subscribe to `water_bucket/state/#` to see level_1/2/3 and pump. Publish to `water_bucket/cmd/pump` with payload `0`, `1`, `2`, `3`, or `off` to drive pumps. Ensure `WB_MQTT_BROKER_URI` in `wb_config.h` matches the machine running the broker.

**Hardware checks:**

1. With all level sensors dry (or pins 32/33/35 high): confirm pump state is `off`; send a pump command via MQTT and confirm it is rejected (no pump turns on).
2. Ground one level pin (or simulate water): send `water_bucket/cmd/pump` with `0` then `1` and confirm only one pump is on at a time and `water_bucket/state/pump` reflects the active pump.
3. Send `off` and confirm all pumps off.


## Home Assistant setup

### Production workflow

1. **Broker:** Settings → Add-ons → Add-on Store → Mosquitto broker. Install, enable "Start on boot" and "Watchdog". Note host (usually HA IP) and port (1883). If you enable auth, create a user and use it in `wb_config.h`.
2. **Integration:** Settings → Integrations → Add → MQTT. Configure broker (host, port, user/password if used). Set `WB_MQTT_BROKER_URI` in `wb_config.h` to match (e.g. `mqtt://192.168.1.10:1883`).
3. **Discovery:** Leave MQTT discovery enabled (Settings → Devices & Services → MQTT → Configure). Default prefix `homeassistant` is used.
4. **Flash device:** Build and flash the ESP32. Once it connects to WiFi and MQTT, it publishes discovery (retained) and a birth message on `water_bucket/status` (`online`). LWT is set so on unexpected disconnect the broker publishes `offline` to `water_bucket/status`.
5. **Device in HA:** After the ESP32 connects, one device **Water Bucket** appears under MQTT with: three binary sensors (Level 1–3), four switches (Pump 0–3), and availability from `water_bucket/status`. Entities show *unavailable* when the device is offline.

### MQTT discovery (firmware)

The firmware follows Home Assistant MQTT discovery best practices:

- **Device:** Single device per ESP32; `identifiers` use MAC-based id (`water_bucket_<mac_hex>`), `name`, `model`, `manufacturer` for the device card.
- **Availability:** All entities use `availability_topic`: `water_bucket/status`, `payload_available`: `online`, `payload_not_available`: `offline`. The ESP32 publishes `online` (retained) on connect and sets LWT `offline` (retained) so HA marks the device unavailable when the connection drops.
- **Binary sensors:** Level 1–3; `state_topic` per level, `payload_on`: `1` (dry), `payload_off`: `0` (water).
- **Pump control:** Four MQTT **switches** (Pump 0, Pump 1, Pump 2, Pump 3). Each switch: `command_topic` `water_bucket/cmd/pump`, `payload_on` `0`–`3`, `payload_off` `off`; `state_topic` `water_bucket/state/pump` with `value_template` so the switch is on only when that pump index is active. Only one pump can be on; turning another on switches the active pump; turning a switch off sends `off`.

No `configuration.yaml` is required when discovery is used.

### Entities (manual fallback)

If discovery is disabled or entities do not appear, add them in `configuration.yaml` under the top-level `mqtt:` key. Then restart Home Assistant.

```yaml
mqtt:
  binary_sensor:
    - name: "Water bucket level 1"
      state_topic: "water_bucket/state/level_1"
      payload_on: "1"
      payload_off: "0"
      availability_topic: "water_bucket/status"
      payload_available: "online"
      payload_not_available: "offline"
      unique_id: "water_bucket_level_1"
    - name: "Water bucket level 2"
      state_topic: "water_bucket/state/level_2"
      payload_on: "1"
      payload_off: "0"
      availability_topic: "water_bucket/status"
      payload_available: "online"
      payload_not_available: "offline"
      unique_id: "water_bucket_level_2"
    - name: "Water bucket level 3"
      state_topic: "water_bucket/state/level_3"
      payload_on: "1"
      payload_off: "0"
      availability_topic: "water_bucket/status"
      payload_available: "online"
      payload_not_available: "offline"
      unique_id: "water_bucket_level_3"
  switch:
    - name: "Water bucket pump 0"
      command_topic: "water_bucket/cmd/pump"
      state_topic: "water_bucket/state/pump"
      payload_on: "0"
      payload_off: "off"
      value_template: "{{ 'ON' if value == '0' else 'OFF' }}"
      state_on: "ON"
      state_off: "OFF"
      availability_topic: "water_bucket/status"
      payload_available: "online"
      payload_not_available: "offline"
      unique_id: "water_bucket_pump_0"
    - name: "Water bucket pump 1"
      command_topic: "water_bucket/cmd/pump"
      state_topic: "water_bucket/state/pump"
      payload_on: "1"
      payload_off: "off"
      value_template: "{{ 'ON' if value == '1' else 'OFF' }}"
      state_on: "ON"
      state_off: "OFF"
      availability_topic: "water_bucket/status"
      payload_available: "online"
      payload_not_available: "offline"
      unique_id: "water_bucket_pump_1"
    - name: "Water bucket pump 2"
      command_topic: "water_bucket/cmd/pump"
      state_topic: "water_bucket/state/pump"
      payload_on: "2"
      payload_off: "off"
      value_template: "{{ 'ON' if value == '2' else 'OFF' }}"
      state_on: "ON"
      state_off: "OFF"
      availability_topic: "water_bucket/status"
      payload_available: "online"
      payload_not_available: "offline"
      unique_id: "water_bucket_pump_2"
    - name: "Water bucket pump 3"
      command_topic: "water_bucket/cmd/pump"
      state_topic: "water_bucket/state/pump"
      payload_on: "3"
      payload_off: "off"
      value_template: "{{ 'ON' if value == '3' else 'OFF' }}"
      state_on: "ON"
      state_off: "OFF"
      availability_topic: "water_bucket/status"
      payload_available: "online"
      payload_not_available: "offline"
      unique_id: "water_bucket_pump_3"
```

If you already have an `mqtt:` block, merge the `binary_sensor:` and `switch:` lists into it; do not add a second `mqtt:` key.

### Topic list (automations / debugging)

| Topic | Payload | Direction |
|-------|---------|-----------|
| water_bucket/status | online or offline | ESP32 → HA (birth/LWT) |
| water_bucket/state/level_1 | 0 or 1 | ESP32 → HA |
| water_bucket/state/level_2 | 0 or 1 | ESP32 → HA |
| water_bucket/state/level_3 | 0 or 1 | ESP32 → HA |
| water_bucket/state/pump | 0, 1, 2, 3, or off | ESP32 → HA |
| water_bucket/cmd/pump | 0, 1, 2, 3, or off | HA → ESP32 |

### Dashboard

Add a dashboard (e.g. Settings → Dashboards → Add dashboard) or a new tab on an existing one. In YAML mode, use the following as a starting point. Replace the entity IDs with yours (Settings → Devices & Services → Entities; filter by "Water Bucket" or search for `water_bucket`). Entity IDs are usually `binary_sensor.water_bucket_level_1`, `switch.water_bucket_pump_0`, etc.; multiple devices may have a numeric suffix (e.g. `switch.water_bucket_pump_0_2`).

```yaml
title: Water Bucket
path: water-bucket
icon: mdi:water-pump
cards:
  - type: entities
    title: Level sensors
    entities:
      - entity: binary_sensor.water_bucket_level_1
        name: Level 1
      - entity: binary_sensor.water_bucket_level_2
        name: Level 2
      - entity: binary_sensor.water_bucket_level_3
        name: Level 3
  - type: horizontal-stack
    cards:
      - type: entity
        entity: switch.water_bucket_pump_0
        name: Pump 0
      - type: entity
        entity: switch.water_bucket_pump_1
        name: Pump 1
      - type: entity
        entity: switch.water_bucket_pump_2
        name: Pump 2
      - type: entity
        entity: switch.water_bucket_pump_3
        name: Pump 3
```

For a grid layout (e.g. levels in one row, pumps in another), use `type: grid` with `columns` and put the entities and horizontal-stack cards in `cards`. You can add a **Glance** or **Entities** card showing the device status (availability) or use a **Markdown** card with instructions. To show “Dry”/“Water” instead of on/off for levels, use a **Template** entity or a custom card; the default binary sensor shows on (dry) / off (water).
