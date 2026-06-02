#ifndef CONFIG_H
#define CONFIG_H

#define REASON_SUCCESS 1
#define REASON_FP_FAIL 2
#define REASON_FACE_FAIL 3

// ── Fingerprint Sensor (AS608/R307) ──────────────────────
#define FP_RX_PIN        50
#define FP_TX_PIN        51

// ── OLED (SSD1306 128x64, I2C) ───────────────────────────
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64
#define OLED_ADDR        0x3C

// ── Buttons ──────────────────────────────────────────────
#define BTN_ADMIN        2
#define BTN_ENROLL       3
#define BTN_VIEW_LOG     4

// ── Outputs ──────────────────────────────────────────────
#define RELAY_PIN        34
#define BUZZER_PIN       26
#define LED_GREEN        30
#define LED_RED          28

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

// ── Function Prototypes ──────────────────────────────────
void oledInit();
void oledShow(const char* line1, const char* line2 = "", const char* line3 = "");
void oledShowLarge(const char* text);

#endif