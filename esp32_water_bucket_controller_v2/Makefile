IDF_PY ?= idf.py
PORT ?= $(IDF_PORT)
SERIAL_OPTS = $(if $(PORT),-p $(PORT),)

.PHONY: build flash monitor watch clean fullclean help set-target

build: main/wb_config.h
	$(IDF_PY) build

flash: main/wb_config.h
	$(IDF_PY) $(SERIAL_OPTS) flash

monitor:
	$(IDF_PY) $(SERIAL_OPTS) monitor

watch: main/wb_config.h
	$(IDF_PY) build
	$(IDF_PY) $(SERIAL_OPTS) flash monitor

clean:
	$(IDF_PY) fullclean

fullclean: clean

set-target:
	$(IDF_PY) set-target esp32

help:
	@echo "Water Bucket Controller (ESP32) - ESP-IDF"
	@echo ""
	@echo "Targets:"
	@echo "  build     Build firmware (default)"
	@echo "  flash     Flash firmware to device"
	@echo "  monitor   Open serial monitor"
	@echo "  watch     Build, flash, then monitor (development loop)"
	@echo "  clean     fullclean build artifacts"
	@echo "  set-target Run idf.py set-target esp32 (one-time)"
	@echo "  help      This message"
	@echo ""
	@echo "Variables:"
	@echo "  PORT      Serial port (e.g. /dev/cu.usbserial-* or COM3). Optional; idf.py may auto-detect."
	@echo "  IDF_PY    Path to idf.py (default: idf.py)"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make flash PORT=/dev/cu.usbserial-0001"
	@echo "  make watch PORT=/dev/cu.usbserial-0001"

main/wb_config.h:
	@echo "Error: main/wb_config.h not found."
	@echo "Copy main/wb_config.h.example to main/wb_config.h and set WB_WIFI_SSID, WB_WIFI_PASSWORD, WB_MQTT_BROKER_URI."
	@exit 1
