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
#include <RTClib.h>          // DS3231 real-time clock

#define FINGER_BAUD   57600
#define BTN_LOGIN     42
#define BTN_ENROLL    43
#define BTN_SILENCE   44
#define BTN_CANCEL    45
#define BTN_RESET_DB  46
#define BUZZER_PIN    26
#define RELAY_PIN     40
#define LED_GREEN     30
#define LED_RED       28
#define LED_PROCESS   32   // ON while a login/enroll is in progress

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
RTC_DS3231 rtc;

bool alarmActive = false;
bool rtcOk = false;       // true if the DS3231 was found

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

// ── RTC time helpers ─────────────────────────────────────────

// Full timestamp for logging: "2026-06-14 14:32:05"
void getDateTime(char* buf, size_t n) {
  if (!rtcOk) { snprintf(buf, n, "0000-00-00 00:00:00"); return; }
  DateTime now = rtc.now();
  snprintf(buf, n, "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
}

// Short clock for the OLED: "14:32"
void getClock(char* buf, size_t n) {
  if (!rtcOk) { snprintf(buf, n, "--:--"); return; }
  DateTime now = rtc.now();
  snprintf(buf, n, "%02d:%02d", now.hour(), now.minute());
}

// Send an access event to the PC database (via the ESP32-CAM) stamped
// with the DS3231 time:  "LOG|2026-06-14 14:32:05|GRANTED|1"
void sendLog(const char* event, int userId) {
  char ts[24];
  getDateTime(ts, sizeof(ts));
  CAM_SERIAL.print("LOG|");
  CAM_SERIAL.print(ts);
  CAM_SERIAL.print('|');
  CAM_SERIAL.print(event);
  CAM_SERIAL.print('|');
  CAM_SERIAL.println(userId);
  Serial.print(F("  [LOG] ")); Serial.print(ts);
  Serial.print(F(" ")); Serial.print(event);
  Serial.print(F(" id=")); Serial.println(userId);
}

void showWelcome() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  
  // Header: title + live clock
  oled.setCursor(2, 0);
  oled.print(F("BIOMETRIC"));
  char clk[8];
  getClock(clk, sizeof(clk));
  oled.setCursor(92, 0);
  oled.println(clk);
  oled.drawFastHLine(0, 9, 128, SSD1306_WHITE);
  
  // Grid of 5 buttons
  oled.setCursor(0, 14);  oled.println(F("1:Login"));
  oled.setCursor(0, 26);  oled.println(F("3:Silence"));
  oled.setCursor(0, 38);  oled.println(F("5:Reset DB"));
  
  oled.setCursor(68, 14); oled.println(F("2:Enroll"));
  oled.setCursor(68, 26); oled.println(F("4:Cancel"));
  
  // Footer: template count
  oled.drawFastHLine(0, 51, 128, SSD1306_WHITE);
  oled.setCursor(10, 55);
  char countBuf[24];
  sprintf(countBuf, "Templates: %d", finger.templateCount);
  oled.println(countBuf);
  
  oled.display();
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
  pinMode(BTN_RESET_DB, INPUT_PULLUP);
  pinMode(BUZZER_PIN,  OUTPUT);
  pinMode(RELAY_PIN,   OUTPUT);
  pinMode(LED_GREEN,   OUTPUT);
  pinMode(LED_RED,     OUTPUT);
  pinMode(LED_PROCESS, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN,  HIGH);  // HIGH = locked (active-LOW relay)
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_RED,    LOW);
  digitalWrite(LED_PROCESS, LOW);

  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  oled.clearDisplay();
  oled.display();

  // ── RTC (DS3231) ──
  rtcOk = rtc.begin();
  if (!rtcOk) {
    Serial.println(F("[RTC] DS3231 not found!"));
  } else if (rtc.lostPower()) {
    // Set the clock to this sketch's compile time once after a battery loss.
    Serial.println(F("[RTC] lost power -> setting to compile time."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

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
      digitalWrite(LED_PROCESS, HIGH);   // busy
      searchFlow();
      digitalWrite(LED_PROCESS, LOW);
      while (digitalRead(BTN_LOGIN) == LOW) { delay(10); }
    }
  }

  if (digitalRead(BTN_ENROLL) == LOW) {
    delay(50);
    if (digitalRead(BTN_ENROLL) == LOW) {
      digitalWrite(LED_PROCESS, HIGH);   // busy
      enrollFlow();
      digitalWrite(LED_PROCESS, LOW);
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
    if (digitalRead(BTN_RESET_DB) == LOW) {
    delay(50);
    if (digitalRead(BTN_RESET_DB) == LOW) {
        oledShow("Hold Button 5", "for 3 seconds to", "reset database...");
        unsigned long t = millis();
        bool triggered = false;
        while (digitalRead(BTN_RESET_DB) == LOW) {
            if (millis() - t > 3000) {
                triggered = true;
                break;
            }
            delay(10);
        }
        if (triggered) {
            oledShow("DB Resetting...", "1/2 Clearing finger");
            Serial.println(F("\n>> RESET DATABASE"));
            
            // Step 1: Clear local fingerprint sensor database
            uint8_t f_res = finger.emptyDatabase();
            if (f_res == FINGERPRINT_OK) {
                Serial.println(F("  [FINGER] Cleared fingerprint DB."));
                oledShow("DB Resetting...", "2/2 Clearing face DB");
            } else {
                Serial.print(F("  [FINGER] emptyDatabase failed: ")); Serial.println(f_res);
                Serial.println(F("  [FINGER] Attempting manual delete loop..."));
                // Fallback: manually delete all possible template slots with OLED progress
                for (int id = 1; id <= 127; id++) {
                    char progressMsg[24];
                    sprintf(progressMsg, "Deleting slot %d/127", id);
                    oledShow("DB Resetting...", "1/2 Clearing finger", progressMsg);
                    finger.deleteModel(id);
                }
                Serial.println(F("  [FINGER] Manual clear finished."));
                oledShow("DB Resetting...", "2/2 Clearing face DB");
            }
            finger.getTemplateCount(); // Update template count to 0
            delay(1000);

            // Step 2: Clear remote face database on PC server via ESP32-CAM
            if (requestFaceReset()) {
                oledShow("DB Reset!", "Done.");
                Serial.println(F("  [SYS] Database reset complete."));
            } else {
                oledShow("Reset Error!", "Face DB clear failed");
                Serial.println(F("  [SYS] Face database reset failed."));
            }
            delay(2500);
            showWelcome();
        } else {
            // Cancelled if released before 3s
            oledShow("Cancelled.");
            delay(1000);
            showWelcome();
        }
        while (digitalRead(BTN_RESET_DB) == LOW) { delay(10); }
    }
  }

  // Keep the welcome-screen clock current (refresh while idle).
  static unsigned long lastClock = 0;
  if (!alarmActive && millis() - lastClock > 20000) {
    lastClock = millis();
    showWelcome();
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
// Shows the alarm time on the OLED and logs it with the RTC timestamp.
void triggerAlarm(const char* title, int userId = 0) {
  char clk[8];  getClock(clk, sizeof(clk));
  char line[22]; snprintf(line, sizeof(line), "ALARM  %s", clk);
  alarmActive = true;
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(LED_RED,    HIGH);
  digitalWrite(LED_PROCESS, LOW);   // blue off so red shows clean
  Serial.print(F("[ALARM] ")); Serial.println(title);
  oledShow(title, line, "Press 44 to silence.");
  sendLog(title, userId);           // log alarm reason + RTC time
}

// Open the door for 3s on a successful 2-factor login.
// Shows the user id + login time and logs it.
void grantAccess(int userId) {
  char clk[8];  getClock(clk, sizeof(clk));
  char line[22]; snprintf(line, sizeof(line), "ID:%d   %s", userId, clk);
  Serial.print(F("  [GRANTED] ")); Serial.println(line);
  digitalWrite(RELAY_PIN, LOW);   // LOW = unlocked
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_PROCESS, LOW);   // blue off so green shows clean
  oledShow("Access Granted!", line, "Door unlocked...");
  sendLog("GRANTED", userId);
  delay(3000);
  digitalWrite(RELAY_PIN, HIGH);  // HIGH = locked
  digitalWrite(LED_GREEN, LOW);
  oledShow("Door locked.", line);
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

// Ask the ESP32-CAM to trigger a face database reset on the PC server.
// Returns true on success, false on failure/cancel/timeout.
bool requestFaceReset() {
  while (CAM_SERIAL.available()) CAM_SERIAL.read();  // flush stale bytes
  CAM_SERIAL.println("RESET_DB");
  Serial.println(F("  [CAM] RESET_DB sent, waiting..."));

  String resp = "";
  unsigned long t0 = millis();
  while (millis() - t0 < 15000) {  // 15 seconds timeout
    if (digitalRead(BTN_CANCEL) == LOW) return false;
    while (CAM_SERIAL.available()) {
      char c = CAM_SERIAL.read();
      if (c == '\n') {
        resp.trim();
        Serial.print(F("  [CAM] -> ")); Serial.println(resp);
        if (resp == "RESET_OK") return true;
        return false;
      } else if (c != '\r') {
        resp += c;
      }
    }
  }
  return false; // timeout
}

// ── LOGIN (face first, then fingerprint) ─────────────────────

void searchFlow() {
  Serial.println(F("\n>> LOGIN"));

  // ── Factor 1: Face ──
  oledShow("Step 1: Face", "Look at camera...", "(4:Cancel)");
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
  oledShow("Step 2: Finger", "Place finger on", "sensor... (4:Cancel)");

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
      grantAccess(faceID);              // face & finger are the same person
    } else {
      char buf[40];
      sprintf(buf, "Face:%d Finger:%d", faceID, finger.fingerID);
      Serial.print(F("  [DENIED] mismatch -> ")); Serial.println(buf);
      triggerAlarm("ID MISMATCH!", faceID);   // different people -> alarm
    }
  } else if (p == FINGERPRINT_NOTFOUND) {
    triggerAlarm("UNKNOWN USER!", faceID);
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
  oledShow("Register Face", "Look at camera...", "(4:Cancel)");
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
  oledShow("Register", "Place finger on", "sensor... (4:Cancel)");
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
  oledShow("Register", "Place same finger", "again... (4:Cancel)");
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
    char clk[8];  getClock(clk, sizeof(clk));
    char idStr[24]; snprintf(idStr, sizeof(idStr), "ID %d  at %s", id, clk);
    Serial.print(F("Registered ")); Serial.println(idStr);
    oledShow("Registered!", idStr);
    sendLog("REGISTER", id);
    delay(2500);
    showWelcome();
  } else {
    returnToWelcome("Store failed. Try again.");
  }
}