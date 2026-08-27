#pragma once

#include <stdint.h>
#include "driver/gpio.h"

#define OLED_SDA_PIN GPIO_NUM_41
#define OLED_SCL_PIN GPIO_NUM_42
#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

typedef enum {
    FACE_IDLE = 0,
    FACE_LISTENING,
    FACE_THINKING,
    FACE_SPEAKING,
    FACE_HAPPY,
    FACE_SAD,
    FACE_ERROR,
    FACE_SLEEP
} face_state_t;

void oled_init(void);
void display_status(const char* text);
void oled_render_frame(const uint8_t* framebuffer);
void face_set_state(face_state_t state);
face_state_t face_get_state(void);
void face_render(void);
void face_animation_start(void);
