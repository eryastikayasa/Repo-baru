# OLED migration

This branch is based on `feature/wakeword-alexa`.

OLED implementation is enabled from the Gemini project without replacing the Alexa wake-word, audio, Wi-Fi, or WebSocket code.

Hardware: SSD1306 128x64, I2C address 0x3C, SDA GPIO1, SCL GPIO2.

The `main` and `feature/wakeword-alexa` branches remain unchanged by this OLED work.
