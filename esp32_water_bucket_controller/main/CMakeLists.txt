idf_component_register(
    SRCS "main.cpp" "gpio.c" "level.c" "pump.c" "mqtt.c" "wifi.c" "log_tcp.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_gpio esp_wifi espressif__mqtt nvs_flash esp_netif esp_event esp_timer lwip
)
