#ifndef CONFIG_H
#define CONFIG_H

// ── Fingerprint Sensor (AS608/R307) ──────────────────────
#define FP_RX_PIN        10
#define FP_TX_PIN        11

// ── OLED (SSD1306 128x64, I2C) ───────────────────────────
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64
#define OLED_ADDR        0x3C

// ── Buttons ──────────────────────────────────────────────
#define BTN_ADMIN        2
#define BTN_ENROLL       3
#define BTN_VIEW_LOG     4

// ── Outputs ──────────────────────────────────────────────
#define RELAY_PIN        30
#define BUZZER_PIN       31
#define LED_GREEN        32
#define LED_RED          33

// ── UART from ESP32-CAM (Serial1 on Mega) ────────────────
#define CAM_SERIAL       Serial1
#define MSG_FACE_OK      "FACE_OK"
#define MSG_FACE_DENIED  "FACE_DENIED"

// ── EEPROM Layout ────────────────────────────────────────
#define EEPROM_LOG_START    0
#define EEPROM_LOG_COUNT    900
#define MAX_LOG_ENTRIES     100

// ── Timing ───────────────────────────────────────────────
#define LOCK_OPEN_DURATION  5000
#define ENROLL_HOLD_MS      2000

#endif