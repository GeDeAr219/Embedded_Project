#include "config.h"

enum State {
  STATE_IDLE,
  STATE_GRANTED,
  STATE_DENIED,
  STATE_ADMIN
};

State currentState = STATE_IDLE;
String camBuffer = "";

void setup() {
  Serial.begin(9600);
  CAM_SERIAL.begin(9600);

  pinMode(BTN_ADMIN,    INPUT_PULLUP);
  pinMode(BTN_ENROLL,   INPUT_PULLUP);
  pinMode(BTN_VIEW_LOG, INPUT_PULLUP);
  pinMode(RELAY_PIN,    OUTPUT);
  pinMode(BUZZER_PIN,   OUTPUT);
  pinMode(LED_GREEN,    OUTPUT);
  pinMode(LED_RED,      OUTPUT);

  digitalWrite(RELAY_PIN, LOW);

  oledInit();
  fingerprintInit();
  logInit();

  oledShow("System Ready", "Waiting for face...");
  Serial.println("[SYS] Boot complete.");
}

void loop() {
  // Admin mode trigger — hold 2 seconds
  if (digitalRead(BTN_ADMIN) == LOW) {
    delay(ENROLL_HOLD_MS);
    if (digitalRead(BTN_ADMIN) == LOW) {
      adminEnrollmentMode();
      oledShow("System Ready", "Waiting for face...");
    }
  }

  // Print logs to Serial Monitor
  if (digitalRead(BTN_VIEW_LOG) == LOW) {
    delay(50);
    printAllLogs();
    oledShow("Logs printed", "Check Serial");
    delay(2000);
    oledShow("System Ready", "Waiting for face...");
  }

  // Read message from ESP32-CAM via UART
  while (CAM_SERIAL.available()) {
    char c = CAM_SERIAL.read();
    if (c == '\n') {
      camBuffer.trim();
      handleCamMessage(camBuffer);
      camBuffer = "";
    } else {
      camBuffer += c;
    }
  }
}

void handleCamMessage(String msg) {
  Serial.print("[CAM] Received: ");
  Serial.println(msg);

  if (msg == MSG_FACE_OK) {
    oledShow("Face OK!", "Place finger...");
    int fpID = verifyFingerprint();
    if (fpID > 0) {
      grantAccess(fpID);
    } else {
      denyAccess(0, REASON_FP_FAIL);
    }
  } else if (msg == MSG_FACE_DENIED) {
    denyAccess(0, REASON_FACE_FAIL);
  }
}

void grantAccess(uint8_t userID) {
  Serial.println("[SYS] ACCESS GRANTED");
  writeLog(userID, 1, REASON_SUCCESS);

  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  oledShowLarge("GRANTED");
  tone(BUZZER_PIN, 1000, 300);

  delay(LOCK_OPEN_DURATION);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_GREEN, LOW);
  oledShow("System Ready", "Waiting for face...");
}

void denyAccess(uint8_t userID, uint8_t reason) {
  Serial.println("[SYS] ACCESS DENIED");
  writeLog(userID, 0, reason);

  digitalWrite(LED_RED, HIGH);
  oledShowLarge("DENIED");
  tone(BUZZER_PIN, 300, 1000);

  delay(2000);

  digitalWrite(LED_RED, LOW);
  oledShow("System Ready", "Waiting for face...");
}