#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "wb_config.h"

#if WB_LOG_TCP_PORT > 0

static int s_client_sock = -1;
static SemaphoreHandle_t s_client_mux;
static vprintf_like_t s_orig;

static int log_vprintf(const char *fmt, va_list ap)
{
    char buf[256];
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (s_orig) {
        s_orig(fmt, ap2);
    }
    va_end(ap2);
    if (s_client_sock >= 0) {
        xSemaphoreTake(s_client_mux, portMAX_DELAY);
        if (s_client_sock >= 0) {
            send(s_client_sock, buf, (size_t)n, 0);
        }
        xSemaphoreGive(s_client_mux);
    }
    return n;
}

static void log_tcp_task(void *arg)
{
    (void)arg;
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons((uint16_t)WB_LOG_TCP_PORT),
    };
    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(listen_sock);
        return;
    }
    if (listen(listen_sock, 1) != 0) {
        close(listen_sock);
        return;
    }
    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client = accept(listen_sock, (struct sockaddr *)&client_addr, &len);
        if (client < 0) {
            continue;
        }
        xSemaphoreTake(s_client_mux, portMAX_DELAY);
        s_client_sock = client;
        xSemaphoreGive(s_client_mux);
        const char *msg = "\n*** TCP log connected ***\n";
        send(client, msg, (size_t)strlen(msg), 0);
        char discard[64];
        while (recv(client, discard, sizeof(discard), 0) > 0) {
        }
        xSemaphoreTake(s_client_mux, portMAX_DELAY);
        close(s_client_sock);
        s_client_sock = -1;
        xSemaphoreGive(s_client_mux);
    }
}

void log_tcp_init(void)
{
    if (WB_LOG_TCP_PORT <= 0) {
        return;
    }
    s_client_mux = xSemaphoreCreateMutex();
    if (s_client_mux == NULL) {
        return;
    }
    s_orig = esp_log_set_vprintf(log_vprintf);
    xTaskCreate(log_tcp_task, "log_tcp", 3072, NULL, 5, NULL);
}

#else

void log_tcp_init(void)
{
}

#endif
