#include "display.h"
#include "wifi_manager.h"
#include "websocket_mgr.h"
#include "audio_hal.h"
#include "websocket_internal.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_sntp.h"

#include <sys/time.h>
#include <time.h>

#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "esp_psram.h"
#include "esp_heap_caps.h"



static const char *TAG = "MAIN";

// ============================================================
// NETWORK DEBUG
// ============================================================

static bool debug_dns_resolution(void)
{
    const char *host = "generativelanguage.googleapis.com";
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "DEBUG NETWORK START");
    ESP_LOGI(TAG, "DNS test: %s", host);
    struct addrinfo hints = {};
    struct addrinfo *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, "443", &hints, &result);
    if (err != 0) {
        ESP_LOGE(TAG, "DNS FAILED: getaddrinfo error=%d errno=%d", err, errno);
        ESP_LOGI(TAG, "========================================");
        return false;
    }
    ESP_LOGI(TAG, "DNS OK");
    bool found_ipv4 = false;
    for (struct addrinfo *p = result; p != nullptr; p = p->ai_next) {
        if (p->ai_family != AF_INET) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
        char ip[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != nullptr) ESP_LOGI(TAG, "DNS IPv4: %s", ip);
        found_ipv4 = true;
        break;
    }
    freeaddrinfo(result);
    if (!found_ipv4) {
        ESP_LOGE(TAG, "DNS OK tetapi tidak mendapatkan IPv4");
        ESP_LOGI(TAG, "========================================");
        return false;
    }
    ESP_LOGI(TAG, "DNS RESULT: OK");
    ESP_LOGI(TAG, "========================================");
    return true;
}

static bool debug_tcp_connection(void)
{
    const char *host = "generativelanguage.googleapis.com";
    const char *port = "443";
    ESP_LOGI(TAG, "TCP test: %s:%s", host, port);
    struct addrinfo hints = {};
    struct addrinfo *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, port, &hints, &result);
    if (err != 0 || result == nullptr) {
        ESP_LOGE(TAG, "TCP test gagal mendapatkan address: error=%d errno=%d", err, errno);
        return false;
    }
    int sock = -1;
    bool connected = false;
    for (struct addrinfo *p = result; p != nullptr; p = p->ai_next) {
        if (p->ai_family != AF_INET) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
        char ip[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != nullptr) ESP_LOGI(TAG, "TCP target: %s:443", ip);
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "TCP socket() FAILED errno=%d", errno);
            continue;
        }
        struct timeval timeout = {};
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ESP_LOGI(TAG, "TCP connect()...");
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            ESP_LOGI(TAG, "TCP CONNECT OK");
            connected = true;
            close(sock);
            break;
        }
        ESP_LOGE(TAG, "TCP CONNECT FAILED errno=%d", errno);
        close(sock);
        sock = -1;
    }
    freeaddrinfo(result);
    if (connected) {
        ESP_LOGI(TAG, "TCP RESULT: OK");
        return true;
    }
    ESP_LOGE(TAG, "TCP RESULT: GAGAL");
    return false;
}

static void debug_network_path(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "NETWORK DIAGNOSTIC");
    ESP_LOGI(TAG, "Target: generativelanguage.googleapis.com:443");
    ESP_LOGI(TAG, "========================================");
    if (!debug_dns_resolution()) {
        ESP_LOGE(TAG, "NETWORK STOP: DNS");
        ESP_LOGI(TAG, "========================================");
        return;
    }
    if (!debug_tcp_connection()) {
        ESP_LOGE(TAG, "NETWORK STOP: TCP");
        ESP_LOGI(TAG, "DNS = OK");
        ESP_LOGI(TAG, "TCP = FAILED");
        ESP_LOGI(TAG, "TLS = BELUM DITES");
        ESP_LOGI(TAG, "========================================");
        return;
    }
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "NETWORK BASIC TEST = OK");
    ESP_LOGI(TAG, "DNS = OK");
    ESP_LOGI(TAG, "TCP 443 = OK");
    ESP_LOGI(TAG, "NEXT = WebSocket/TLS");
    ESP_LOGI(TAG, "========================================");
}

// ============================================================
// SNTP
// ============================================================

static void sync_sntp_time(void)
{
    ESP_LOGI(TAG, "Mencari server NTP...");
    display_status("Sync Jam Network..");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.google.com");
    esp_sntp_setservername(1, "id.pool.ntp.org");
    esp_sntp_setservername(2, "pool.ntp.org");
    esp_sntp_init();
    int retry = 0;
    const int max_retries = 10;
    time_t now = 0;
    struct tm timeinfo = {};
    while (retry < max_retries) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year >= (2024 - 1900)) {
            ESP_LOGI(TAG, "Waktu cocok! Tahun: %d", timeinfo.tm_year + 1900);
            display_status("Jam Cocok!");
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
    }
    ESP_LOGW(TAG, "NTP gagal. Menggunakan waktu fallback.");
    struct timeval tv = { .tv_sec = 1770000000, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    display_status("Jam Set Fallback");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ============================================================
// AUDIO TASK
// ============================================================

static bool mic_frame_has_activity(const uint8_t *data, size_t len)
{
    if (!data || len < 2) return false;

    constexpr int32_t SILENCE_THRESHOLD = 700;
    constexpr size_t MIN_ACTIVE_SAMPLES = 8;
    size_t active_samples = 0;

    for (size_t i = 0; i + 1 < len; i += 2) {
        int16_t sample = (int16_t)((uint16_t)data[i] | ((uint16_t)data[i + 1] << 8));
        int32_t magnitude = sample < 0 ? -(int32_t)sample : (int32_t)sample;
        if (magnitude >= SILENCE_THRESHOLD) {
            active_samples++;
            if (active_samples >= MIN_ACTIVE_SAMPLES) return true;
        }
    }
    return false;
}

static void audio_task(void *arg)
{
    (void)arg;
    static uint8_t audio_buffer[4096];
    size_t buffer_pos = 0;
    uint32_t silent_frames = 0;
    int64_t last_silent_log_us = 0;

    while (1) {
        if (!websocket_is_connected()) {
            buffer_pos = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        size_t bytes_read = audio_read_mic(audio_buffer + buffer_pos,
                                           sizeof(audio_buffer) - buffer_pos);
        if (bytes_read > 0) buffer_pos += bytes_read;

        if (buffer_pos >= 3200) {
            // Hanya kirim MIC kalau Gemini tidak sedang bicara
            if (!audio_turn_active && mic_frame_has_activity(audio_buffer, 3200)) {
                websocket_send_audio_data(audio_buffer, 3200);
            } else if (!audio_turn_active) {
                silent_frames++;
                int64_t now_us = esp_timer_get_time();
                if (last_silent_log_us == 0 || now_us - last_silent_log_us >= 1000000) {
                    last_silent_log_us = now_us;
                    ESP_LOGI(TAG, "V7.0.36 MIC TX gate: silent frames dropped=%lu",
                             (unsigned long)silent_frames);
                }
            }
            // Kalau audio_turn_active true, frame mic dibuang tanpa log spam

            size_t remainder = buffer_pos - 3200;
            if (remainder > 0) memmove(audio_buffer, audio_buffer + 3200, remainder);
            buffer_pos = remainder;
        }
    }
}

// ============================================================
// APP MAIN
// ============================================================

extern "C" void app_main(void)


 {
    ESP_LOGI("MAIN", "Total PSRAM: %d bytes", esp_psram_get_size());
    ESP_LOGI("MAIN", "Free Heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI("MAIN", "Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
 }

{
    ESP_LOGI(TAG, "ESP32-S3 Asisten Kamar Dimulai...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    oled_init();
    display_status("Booting...");

    audio_hal_init();
    audio_i2s_test_tone();

    display_status("Menghubungkan WiFi...");
    wifi_init_sta();

    if (!wifi_wait_for_connection(15000)) {
        ESP_LOGE(TAG, "Wi-Fi tidak mendapatkan IP.");
        display_status("WiFi Gagal!");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "WIFI READY - lanjut ke NTP");
    sync_sntp_time();
    ESP_LOGI(TAG, "Menunggu 1 detik...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    debug_network_path();
    vTaskDelay(pdMS_TO_TICKS(1000));

    display_status("Menghubungkan AI...");
    websocket_app_start();

    BaseType_t task_result = xTaskCreate(audio_task, "audio_task", 10240, NULL, 5, NULL);
    if (task_result != pdPASS) ESP_LOGE(TAG, "Gagal membuat audio_task!");
    else ESP_LOGI(TAG, "audio_task berhasil dimulai.");

    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
