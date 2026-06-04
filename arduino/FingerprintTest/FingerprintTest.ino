/*
 * Biometric Access — Button + OLED + Face + Fingerprint (2-factor)
 * Pin 42 = Login   (face check -> fingerprint)
 * Pin 43 = Enroll  (Register fingerprint)
 * Pin 44 = Silence Alarm
 * Pin 45 = Cancel
 * Pin 26 = Buzzer (OUTPUT)
 * Pin 40 = Relay / Door Lock (OUTPUT)
 * OLED : SSD1306 128x64 I2C at 0x3C
 * Sensor: AS608/R307 on Serial1 (TX1=18, RX1=19)
 * ESP32-CAM on Serial2 (TX2=16, RX2=17) @115200
 *   Mega sends  "CHECK\n"  -> ESP32-CAM captures + asks PC server
 *   ESP32 replies "FACE_OK" / "FACE_UNKNOWN" / "FACE_NONE" / "FACE_ERR"
 *   NOTE: Mega TX2 (pin16, 5V) -> ESP RX needs a level shifter / divider.
 * Serial Monitor: 115200 baud
 */

#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define FINGER_BAUD   57600
#define BTN_LOGIN     42
#define BTN_ENROLL    43
#define BTN_SILENCE   44
#define BTN_CANCEL    45
#define BUZZER_PIN    26
#define RELAY_PIN     40
#define LED_GREEN     30
#define LED_RED       28

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

// ── ESP32-CAM link (Serial2: TX2=16, RX2=17) ────────────────
#define CAM_SERIAL      Serial2
#define CAM_BAUD        9600   // low baud: cheap level shifters mangle 115200
#define FACE_TIMEOUT    12000   // ms to wait for a CHECK reply
#define FACE_REG_TIMEOUT 25000  // ms to wait for a REGISTER reply (camera retries)

// Face check results
enum FaceResult    { FACE_OK, FACE_UNKNOWN, FACE_NONE, FACE_ERR, FACE_CANCEL };
// Face registration results
enum FaceRegResult { FREG_OK, FREG_FAIL, FREG_ERR, FREG_CANCEL };

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial1);
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

bool alarmActive = false;

// ── OLED helpers ─────────────────────────────────────────────

void oledShow(const char* l1, const char* l2 = "", const char* l3 = "") {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);  oled.println(l1);
  oled.setCursor(0, 22); oled.println(l2);
  oled.setCursor(0, 44); oled.println(l3);
  oled.display();
}

void oledShowLarge(const char* text) {
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 20);
  oled.println(text);
  oled.setTextSize(1);
  oled.display();
}

void showWelcome() {
  oledShow("Welcome!", "42:Login  43:Enroll", "44:Silence 45:Cancel");
}

// ── Setup / Loop ─────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  CAM_SERIAL.begin(CAM_BAUD);

  pinMode(BTN_LOGIN,   INPUT_PULLUP);
  pinMode(BTN_ENROLL,  INPUT_PULLUP);
  pinMode(BTN_SILENCE, INPUT_PULLUP);
  pinMode(BTN_CANCEL,  INPUT_PULLUP);
  pinMode(BUZZER_PIN,  OUTPUT);
  pinMode(RELAY_PIN,   OUTPUT);
  pinMode(LED_GREEN,   OUTPUT);
  pinMode(LED_RED,     OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN,  HIGH);  // HIGH = locked (active-LOW relay)
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_RED,    LOW);

  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  oled.clearDisplay();
  oled.display();

  finger.begin(FINGER_BAUD);
  delay(100);

  if (finger.verifyPassword()) {
    Serial.println(F("[OK] Sensor connected."));
  } else {
    Serial.println(F("[HATA] Sensor not found! Check wiring or try FINGER_BAUD 9600."));
    oledShow("Sensor Error!", "Check wiring.");
    while (true) { delay(1000); }
  }

  finger.getTemplateCount();
  Serial.print(F("Stored fingerprints: "));
  Serial.println(finger.templateCount);

  showWelcome();
}

void loop() {
  if (digitalRead(BTN_LOGIN) == LOW) {
    delay(50);
    if (digitalRead(BTN_LOGIN) == LOW) {
      searchFlow();
      while (digitalRead(BTN_LOGIN) == LOW) { delay(10); }
    }
  }

  if (digitalRead(BTN_ENROLL) == LOW) {
    delay(50);
    if (digitalRead(BTN_ENROLL) == LOW) {
      enrollFlow();
      while (digitalRead(BTN_ENROLL) == LOW) { delay(10); }
    }
  }

  if (digitalRead(BTN_SILENCE) == LOW) {
    delay(50);
    if (digitalRead(BTN_SILENCE) == LOW && alarmActive) {
      alarmActive = false;
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_RED,    LOW);
      Serial.println(F("[ALARM] Silenced."));
      oledShow("Alarm silenced.");
      delay(1500);
      showWelcome();
      while (digitalRead(BTN_SILENCE) == LOW) { delay(10); }
    }
  }
}

// ── Helpers ──────────────────────────────────────────────────

// Waits for a clean finger image. Returns false if cancelled or timed out.
bool waitForFinger() {
  unsigned long t0 = millis();
  int p = FINGERPRINT_NOFINGER;
  while (p != FINGERPRINT_OK) {
    if (digitalRead(BTN_CANCEL) == LOW) return false;
    if (millis() - t0 > 10000)          return false;
    p = finger.getImage();
    delay(50);
  }
  return true;
}

void returnToWelcome(const char* msg, unsigned int ms = 2000) {
  oledShow(msg);
  Serial.println(msg);
  delay(ms);
  showWelcome();
}

// Raise the alarm: buzzer + red LED stay on until button 44.
void triggerAlarm(const char* title) {
  alarmActive = true;
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(LED_RED,    HIGH);
  Serial.print(F("[ALARM] ")); Serial.println(title);
  oledShow(title, "ALARM ACTIVE", "Press 44 to silence.");
}

// Open the door for 3s on a successful 2-factor login.
void grantAccess(const char* info) {
  Serial.print(F("  [GRANTED] ")); Serial.println(info);
  digitalWrite(RELAY_PIN, LOW);   // LOW = unlocked
  digitalWrite(LED_GREEN, HIGH);
  oledShow("Access Granted!", info, "Door unlocked...");
  delay(3000);
  digitalWrite(RELAY_PIN, HIGH);  // HIGH = locked
  digitalWrite(LED_GREEN, LOW);
  oledShow("Door locked.", info);
  delay(1500);
  showWelcome();
}

// ── FACE CHECK (ESP32-CAM over Serial2) ──────────────────────

int matchedFaceID = -1;  // set by requestFaceCheck on FACE_OK (the user id)

// Ask the ESP32-CAM to capture + verify a face against the PC server.
// On FACE_OK, the matched user id is stored in matchedFaceID.
FaceResult requestFaceCheck() {
  matchedFaceID = -1;
  while (CAM_SERIAL.available()) CAM_SERIAL.read();  // flush stale bytes
  CAM_SERIAL.println("CHECK");
  Serial.println(F("  [CAM] CHECK sent, waiting..."));

  String resp = "";
  unsigned long t0 = millis();
  while (millis() - t0 < FACE_TIMEOUT) {
    if (digitalRead(BTN_CANCEL) == LOW) return FACE_CANCEL;
    while (CAM_SERIAL.available()) {
      char c = CAM_SERIAL.read();
      if (c == '\n') {
        resp.trim();
        Serial.print(F("  [CAM] -> ")); Serial.println(resp);
        if (resp.startsWith("FACE_OK")) {     // "FACE_OK:<id>"
          int colon = resp.indexOf(':');
          matchedFaceID = (colon >= 0) ? resp.substring(colon + 1).toInt() : 0;
          return FACE_OK;
        }
        if (resp == "FACE_UNKNOWN") return FACE_UNKNOWN;
        if (resp == "FACE_NONE")    return FACE_NONE;
        return FACE_ERR;
      } else if (c != '\r') {
        resp += c;
      }
    }
  }
  return FACE_ERR;  // timeout
}

// Ask the ESP32-CAM to capture + register the current face as user <id>
// on the PC server. The camera retries a few frames internally.
FaceRegResult requestFaceRegister(int id) {
  while (CAM_SERIAL.available()) CAM_SERIAL.read();  // flush stale bytes
  CAM_SERIAL.print("REGISTER:");
  CAM_SERIAL.println(id);
  Serial.print(F("  [CAM] REGISTER sent for id ")); Serial.println(id);

  String resp = "";
  unsigned long t0 = millis();
  while (millis() - t0 < FACE_REG_TIMEOUT) {
    if (digitalRead(BTN_CANCEL) == LOW) return FREG_CANCEL;
    while (CAM_SERIAL.available()) {
      char c = CAM_SERIAL.read();
      if (c == '\n') {
        resp.trim();
        Serial.print(F("  [CAM] -> ")); Serial.println(resp);
        if (resp == "REG_OK")   return FREG_OK;
        if (resp == "REG_FAIL") return FREG_FAIL;
        return FREG_ERR;
      } else if (c != '\r') {
        resp += c;
      }
    }
  }
  return FREG_ERR;  // timeout
}

// ── LOGIN (face first, then fingerprint) ─────────────────────

void searchFlow() {
  Serial.println(F("\n>> LOGIN"));

  // ── Factor 1: Face ──
  oledShow("Step 1: Face", "Look at camera...", "(45:Cancel)");
  switch (requestFaceCheck()) {
    case FACE_OK:                                     break;  // continue
    case FACE_UNKNOWN: triggerAlarm("UNKNOWN FACE!"); return;
    case FACE_NONE:    returnToWelcome("No face. Try again."); return;
    case FACE_CANCEL:  returnToWelcome("Cancelled.");         return;
    default:           returnToWelcome("Camera error.");      return;
  }
  int faceID = matchedFaceID;  // who the face says you are
  Serial.print(F("  Face matched user ID: ")); Serial.println(faceID);

  // ── Factor 2: Fingerprint (must be the SAME user id) ──
  oledShow("Step 2: Finger", "Place finger on", "sensor... (45:Cancel)");

  if (!waitForFinger()) {
    returnToWelcome("Cancelled.");
    return;
  }

  oledShow("Step 2: Finger", "Scanning...");

  if (finger.image2Tz() != FINGERPRINT_OK) {
    returnToWelcome("Poor image. Try again.");
    return;
  }

  int p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    if ((int)finger.fingerID == faceID) {
      char buf[24];
      sprintf(buf, "User ID:%d OK", faceID);
      grantAccess(buf);                 // face & finger are the same person
    } else {
      char buf[40];
      sprintf(buf, "Face:%d Finger:%d", faceID, finger.fingerID);
      Serial.print(F("  [DENIED] mismatch -> ")); Serial.println(buf);
      triggerAlarm("ID MISMATCH!");     // different people -> alarm
    }
  } else if (p == FINGERPRINT_NOTFOUND) {
    triggerAlarm("UNKNOWN USER!");
  } else {
    returnToWelcome("Search error. Try again.");
  }
}

// ── ENROLL ───────────────────────────────────────────────────

void enrollFlow() {
  finger.getTemplateCount();
  int id = finger.templateCount + 1;
  if (id > 127) {
    returnToWelcome("Sensor full! (127/127)");
    return;
  }

  Serial.print(F("\n>> ENROLL — ID #")); Serial.println(id);

  // ── Step 1: Face registration via camera (same id) ──
  oledShow("Register Face", "Look at camera...", "(45:Cancel)");
  switch (requestFaceRegister(id)) {
    case FREG_OK:                                              break;  // continue
    case FREG_CANCEL: returnToWelcome("Cancelled.");           return;
    case FREG_FAIL:   returnToWelcome("No face. Try again.");  return;
    default:          returnToWelcome("Camera reg error.");    return;
  }
  oledShow("Face saved!", "Now fingerprint...");
  delay(1200);

  // ── Step 2: Fingerprint enrollment (same id) ──
  // 1st scan
  oledShow("Register", "Place finger on", "sensor... (45:Cancel)");
  Serial.println(F("  Place finger (1st scan)..."));

  if (!waitForFinger()) {
    returnToWelcome("Cancelled.");
    return;
  }

  oledShow("Register", "Scanning...");
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    returnToWelcome("Scan failed. Try again.");
    return;
  }

  oledShow("Register", "Lift your finger.");
  Serial.println(F("  1st scan OK. REMOVE finger."));
  delay(1500);
  int p = 0;
  while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); }

  // 2nd scan
  oledShow("Register", "Place same finger", "again... (45:Cancel)");
  Serial.println(F("  Place same finger (2nd scan)..."));

  if (!waitForFinger()) {
    returnToWelcome("Cancelled.");
    return;
  }

  oledShow("Register", "Scanning...");
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    returnToWelcome("2nd scan failed. Try again.");
    return;
  }

  if (finger.createModel() != FINGERPRINT_OK) {
    returnToWelcome("No match! Try again.");
    return;
  }

  if (finger.storeModel(id) == FINGERPRINT_OK) {
    char idStr[24];
    sprintf(idStr, "Registered to ID %d", id);
    Serial.println(idStr);
    oledShow("Registered!", idStr);
    delay(2500);
    showWelcome();
  } else {
    returnToWelcome("Store failed. Try again.");
  }
}
