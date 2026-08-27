#include "display.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "DISPLAY";
static i2c_master_bus_handle_t display_i2c_bus = NULL;
static esp_lcd_panel_io_handle_t panel_io = NULL;
static esp_lcd_panel_handle_t panel = NULL;
static SemaphoreHandle_t oled_mutex = NULL;
static bool oled_ready = false;
static face_state_t current_face_state = FACE_IDLE;
static uint8_t face_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

static void oled_send_cmd(uint8_t cmd)
{
    if (!panel_io) return;
    esp_lcd_panel_io_tx_param(panel_io, 0, &cmd, 1);
}

static void oled_send_data(const uint8_t *data, size_t len)
{
    if (!panel_io || !data || len == 0) return;
    esp_lcd_panel_io_tx_color(panel_io, 0, data, len);
}

static void oled_clear_locked(void)
{
    uint8_t zero[OLED_WIDTH] = {0};
    for (uint8_t page = 0; page < 8; ++page) {
        oled_send_cmd((uint8_t)(0xB0 + page));
        oled_send_cmd(0x00);
        oled_send_cmd(0x10);
        oled_send_data(zero, sizeof(zero));
    }
}

void oled_init(void)
{
    if (oled_ready) return;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = OLED_SDA_PIN,
        .scl_io_num = OLED_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &display_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal membuat I2C OLED: %s", esp_err_to_name(err));
        return;
    }

    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = OLED_I2C_ADDR,
        .scl_speed_hz = 400 * 1000,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 0,
        },
    };

    err = esp_lcd_new_panel_io_i2c(display_i2c_bus, &io_config, &panel_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal membuat OLED panel IO: %s", esp_err_to_name(err));
        return;
    }

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = GPIO_NUM_NC;
    panel_config.bits_per_pixel = 1;

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = OLED_HEIGHT,
    };
    panel_config.vendor_config = &ssd1306_config;

    err = esp_lcd_new_panel_ssd1306(panel_io, &panel_config, &panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal membuat SSD1306 panel: %s", esp_err_to_name(err));
        return;
    }

    err = esp_lcd_panel_reset(panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal reset SSD1306: %s", esp_err_to_name(err));
        return;
    }

    err = esp_lcd_panel_init(panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal init SSD1306: %s", esp_err_to_name(err));
        return;
    }

    err = esp_lcd_panel_invert_color(panel, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal set invert SSD1306: %s", esp_err_to_name(err));
        return;
    }

    err = esp_lcd_panel_disp_on_off(panel, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal menyalakan SSD1306: %s", esp_err_to_name(err));
        return;
    }

    oled_mutex = xSemaphoreCreateMutex();
    if (!oled_mutex) {
        ESP_LOGE(TAG, "Gagal membuat mutex OLED");
        return;
    }

    xSemaphoreTake(oled_mutex, portMAX_DELAY);
    oled_clear_locked();
    xSemaphoreGive(oled_mutex);

    oled_ready = true;
    ESP_LOGI(TAG, "OLED SSD1306 siap: SDA=%d SCL=%d ADDR=0x%02X 128x64 @ 400kHz",
             OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_ADDR);
}

void display_status(const char* text)
{
    if (!oled_ready) oled_init();
    ESP_LOGI(TAG, "[OLED STATUS]: %s", text ? text : "(null)");
    if (!oled_ready || !oled_mutex) return;

    xSemaphoreTake(oled_mutex, portMAX_DELAY);
    oled_clear_locked();
    xSemaphoreGive(oled_mutex);
}

void oled_render_frame(const uint8_t* framebuffer)
{
    if (!oled_ready) oled_init();
    if (!oled_ready || !oled_mutex || !framebuffer) return;

    xSemaphoreTake(oled_mutex, portMAX_DELAY);
    for (uint8_t page = 0; page < 8; ++page) {
        oled_send_cmd((uint8_t)(0xB0 + page));
        oled_send_cmd(0x00);
        oled_send_cmd(0x10);
        oled_send_data(&framebuffer[page * OLED_WIDTH], OLED_WIDTH);
    }
    xSemaphoreGive(oled_mutex);
}

void face_set_state(face_state_t state)
{
    current_face_state = state;
}

face_state_t face_get_state(void)
{
    return current_face_state;
}

static void face_pixel(int x, int y)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    face_buffer[x + (y >> 3) * OLED_WIDTH] |= (uint8_t)(1U << (y & 7));
}

static void face_line(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        face_pixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void face_circle(int cx, int cy, int r)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) face_pixel(cx + x, cy + y);
}

static void face_ellipse(int cx, int cy, int rx, int ry)
{
    for (int y = -ry; y <= ry; y++)
        for (int x = -rx; x <= rx; x++)
            if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry)
                face_pixel(cx + x, cy + y);
}

void face_render(void)
{
    if (!oled_ready) oled_init();
    if (!oled_ready) return;

    memset(face_buffer, 0, sizeof(face_buffer));

    if (current_face_state == FACE_SLEEP) {
        face_line(24, 32, 48, 32);
        face_line(80, 32, 104, 32);
    } else {
        face_ellipse(38, 30, 14, 20);
        face_ellipse(90, 30, 14, 20);
        face_circle(38, 30, 2);
        face_circle(90, 30, 2);

        if (current_face_state == FACE_SPEAKING) {
            face_ellipse(64, 51, 7, 4);
        } else if (current_face_state == FACE_HAPPY) {
            face_line(53, 51, 59, 54);
            face_line(59, 54, 69, 54);
            face_line(69, 54, 75, 51);
        } else if (current_face_state == FACE_SAD) {
            face_line(55, 55, 64, 51);
            face_line(64, 51, 73, 55);
        } else if (current_face_state == FACE_ERROR) {
            face_line(56, 51, 72, 57);
            face_line(72, 51, 56, 57);
        }
    }

    oled_render_frame(face_buffer);
}
