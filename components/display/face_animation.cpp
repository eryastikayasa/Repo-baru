#include "display.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>
#include <string.h>

static const char *TAG = "FACE_ANIM";
static TaskHandle_t anim_task_handle = NULL;
static SemaphoreHandle_t anim_start_mutex = NULL;
static uint8_t anim_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

static uint32_t rnd(uint32_t max_value)
{
    if (max_value == 0) return 0;
    return esp_random() % max_value;
}

static void anim_pixel(int x, int y)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    anim_buffer[x + (y >> 3) * OLED_WIDTH] |= (uint8_t)(1U << (y & 7));
}

static void anim_line(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        anim_pixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void anim_ellipse(int cx, int cy, int rx, int ry)
{
    const int points = 48;
    for (int i = 0; i < points; ++i) {
        float a = (float)i * 6.28318530718f / (float)points;
        int x = cx + (int)lroundf(cosf(a) * (float)rx);
        int y = cy + (int)lroundf(sinf(a) * (float)ry);
        anim_pixel(x, y);
    }
}

static void anim_filled_circle(int cx, int cy, int r)
{
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r * r) anim_pixel(cx + x, cy + y);
}

static void anim_eye(int base_x, int base_y, int move_x, int move_y, bool closed)
{
    const int rx = 14, ry = 20;
    int cx = base_x + move_x;
    int cy = base_y + move_y;
    if (closed) {
        anim_line(cx - 11, cy + 1, cx - 6, cy + 4);
        anim_line(cx - 6, cy + 4, cx, cy + 5);
        anim_line(cx, cy + 5, cx + 6, cy + 4);
        anim_line(cx + 6, cy + 4, cx + 11, cy + 1);
        return;
    }
    anim_ellipse(cx, cy, rx, ry);
    anim_filled_circle(cx, cy, 2);
}

static void anim_upload(void)
{
    oled_render_frame(anim_buffer);
}

static void anim_draw_frame(int eye_move_x, int eye_move_y, bool closed, face_state_t state)
{
    memset(anim_buffer, 0, sizeof(anim_buffer));
    anim_eye(38, 30, eye_move_x, eye_move_y, closed);
    anim_eye(90, 30, eye_move_x, eye_move_y, closed);

    if (state == FACE_SPEAKING) {
        anim_ellipse(64, 51, 7, 4);
    } else if (state == FACE_HAPPY) {
        anim_line(53, 51, 59, 54);
        anim_line(59, 54, 69, 54);
        anim_line(69, 54, 75, 51);
    } else if (state == FACE_SAD) {
        anim_line(55, 55, 64, 51);
        anim_line(64, 51, 73, 55);
    } else if (state == FACE_ERROR) {
        anim_line(56, 51, 72, 57);
        anim_line(72, 51, 56, 57);
    }
    anim_upload();
}

static void anim_move_smooth(int from_x, int from_y, int to_x, int to_y,
                             uint32_t duration_ms, face_state_t state)
{
    const int step_ms = 25;
    int steps = (int)(duration_ms / step_ms);
    if (steps < 1) steps = 1;

    for (int i = 1; i <= steps; ++i) {
        int x = from_x + ((to_x - from_x) * i) / steps;
        int y = from_y + ((to_y - from_y) * i) / steps;
        anim_draw_frame(x, y, false, state);
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }
}

static void anim_blink(face_state_t state, bool double_blink)
{
    anim_draw_frame(0, 0, true, state);
    vTaskDelay(pdMS_TO_TICKS(90));
    anim_draw_frame(0, 0, false, state);
    vTaskDelay(pdMS_TO_TICKS(110));
    if (!double_blink) return;
    anim_draw_frame(0, 0, true, state);
    vTaskDelay(pdMS_TO_TICKS(100));
    anim_draw_frame(0, 0, false, state);
}

static void anim_idle_sequence(void)
{
    vTaskDelay(pdMS_TO_TICKS(3000 + rnd(4001)));
    uint32_t behavior = rnd(100);

    if (behavior < 45) {
        const int target_x = (rnd(2) == 0) ? -6 : 6;
        anim_move_smooth(0, 0, target_x, 0, 180, FACE_IDLE);
        vTaskDelay(pdMS_TO_TICKS(180 + rnd(121)));
        anim_move_smooth(target_x, 0, 0, 0, 180, FACE_IDLE);
        return;
    }

    if (behavior < 70) {
        const int target_y = (rnd(2) == 0) ? -5 : 5;
        anim_move_smooth(0, 0, 0, target_y, 170, FACE_IDLE);
        vTaskDelay(pdMS_TO_TICKS(180 + rnd(121)));
        anim_move_smooth(0, target_y, 0, 0, 170, FACE_IDLE);
        return;
    }

    if (behavior < 85) {
        const int target_x = (rnd(2) == 0) ? -5 : 5;
        const int target_y = (rnd(2) == 0) ? -4 : 4;
        anim_move_smooth(0, 0, target_x, target_y, 180, FACE_IDLE);
        vTaskDelay(pdMS_TO_TICKS(180 + rnd(121)));
        anim_move_smooth(target_x, target_y, 0, 0, 180, FACE_IDLE);
        return;
    }

    anim_blink(FACE_IDLE, rnd(5) == 0);
}

static void anim_state_frame(face_state_t state)
{
    switch (state) {
    case FACE_LISTENING:
        anim_draw_frame(0, 0, false, state);
        vTaskDelay(pdMS_TO_TICKS(80));
        break;

    case FACE_THINKING:
        anim_move_smooth(0, 0, -4, -2, 180, state);
        vTaskDelay(pdMS_TO_TICKS(180));
        anim_move_smooth(-4, -2, 4, -2, 360, state);
        vTaskDelay(pdMS_TO_TICKS(180));
        anim_move_smooth(4, -2, 0, 0, 180, state);
        break;

    case FACE_SPEAKING:
        anim_draw_frame(0, 0, false, state);
        vTaskDelay(pdMS_TO_TICKS(120));
        anim_draw_frame(0, 0, false, state);
        vTaskDelay(pdMS_TO_TICKS(120));
        break;

    case FACE_HAPPY:
        anim_move_smooth(0, 0, 0, -1, 120, state);
        vTaskDelay(pdMS_TO_TICKS(180));
        anim_move_smooth(0, -1, 0, 0, 120, state);
        break;

    case FACE_SAD:
        anim_move_smooth(0, 0, 0, 2, 150, state);
        vTaskDelay(pdMS_TO_TICKS(200));
        anim_move_smooth(0, 2, 0, 0, 150, state);
        break;

    case FACE_ERROR:
        anim_draw_frame(0, 0, false, state);
        vTaskDelay(pdMS_TO_TICKS(100));
        anim_draw_frame(1, 0, false, state);
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

    case FACE_SLEEP:
        anim_draw_frame(0, 0, true, state);
        vTaskDelay(pdMS_TO_TICKS(500));
        break;

    case FACE_IDLE:
    default:
        anim_idle_sequence();
        break;
    }
}

static void face_animation_task(void *arg)
{
    (void)arg;
    while (1) {
        face_state_t state = face_get_state();
        anim_state_frame(state);
    }
}

void face_animation_start(void)
{
    if (anim_start_mutex == NULL) {
        anim_start_mutex = xSemaphoreCreateMutex();
        if (anim_start_mutex == NULL) {
            ESP_LOGE(TAG, "Gagal membuat mutex animasi OLED");
            return;
        }
    }

    xSemaphoreTake(anim_start_mutex, portMAX_DELAY);

    if (anim_task_handle != NULL) {
        xSemaphoreGive(anim_start_mutex);
        return;
    }

    oled_init();

    BaseType_t ok = xTaskCreate(
        face_animation_task,
        "face_anim",
        4096,
        NULL,
        2,
        &anim_task_handle
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Gagal membuat task animasi OLED");
        anim_task_handle = NULL;
        xSemaphoreGive(anim_start_mutex);
        return;
    }

    ESP_LOGI(TAG, "OLED life animation aktif: single esp-lcd panel, oval+pupil bergerak bersama, blink natural 3-7s");
    xSemaphoreGive(anim_start_mutex);
}
