#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================
// GEMINI API KEY
// ============================================================
// Untuk sementara gunakan API key pengujian kamu di sini.
//
// CATATAN:
// Jangan upload API key ke repository publik.
// ============================================================

#define GEMINI_API_KEY 

// ============================================================
// GEMINI LIVE API WEBSOCKET
// ============================================================

#define WEBSOCKET_SERVER_URL \
    "wss://generativelanguage.googleapis.com/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent?key=" \
    GEMINI_API_KEY

void websocket_app_start(void);

void websocket_send_audio_data(const uint8_t *data, size_t len);

bool websocket_is_connected(void);
