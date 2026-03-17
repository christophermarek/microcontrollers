idf_component_register(
    SRCS "lcd.c" "rotary_encoder.c" "ui_test.c" "ota.c" "main.cpp" "gpio.c" "level.c" "pump.c" "mqtt.c" "wifi.c" "log_tcp.c"
    INCLUDE_DIRS "."
    REQUIRES app_update esp_https_ota driver esp_driver_gpio esp_wifi espressif__mqtt nvs_flash esp_netif esp_event esp_timer lwip esp_lcd
)
