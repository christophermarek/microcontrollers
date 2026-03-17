idf_component_register(
    SRCS "ui/ui.c" "ui/ui_pages.c" "ui/ui_render.c" "ui/ui_log.c" "ui/ui_tz.c" "ui/pages/home_page.c" "ui/pages/pumps_page.c" "ui/pages/sensors_page.c" "ui/pages/logs_page.c" "ui/pages/settings_page.c" "lcd.c" "rotary_encoder.c" "ui_test.c" "ota.c" "main.cpp" "gpio.c" "level.c" "pump.c" "mqtt.c" "wifi.c" "log_tcp.c"
    INCLUDE_DIRS "." "ui"
    REQUIRES app_update esp_https_ota driver esp_driver_gpio esp_wifi espressif__mqtt nvs_flash esp_netif esp_event esp_timer lwip esp_lcd
)
