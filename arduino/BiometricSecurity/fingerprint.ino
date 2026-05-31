#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include "config.h"

SoftwareSerial fpSerial(FP_RX_PIN, FP_TX_PIN);
Adafruit_Fingerprint finger(&fpSerial);

void fingerprintInit() {
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("[FP] Sensor found.");
  } else {
    Serial.println("[FP] Sensor NOT found! Check wiring.");
  }
}

// Returns matched ID (1-127) or -1 on failure
int verifyFingerprint() {
  oledShow("Place finger", "on sensor...");

  unsigned long start = millis();
  while (millis() - start < 8000) {
    int result = finger.getImage();
    if (result != FINGERPRINT_OK) continue;

    result = finger.image2Tz();
    if (result != FINGERPRINT_OK) continue;

    result = finger.fingerSearch();
    if (result == FINGERPRINT_OK) {
      Serial.print("[FP] Match! ID: ");
      Serial.println(finger.fingerID);
      return finger.fingerID;
    } else {
      Serial.println("[FP] No match.");
      return -1;
    }
  }
  Serial.println("[FP] Timeout.");
  return -1;
}

bool enrollFingerprint(uint8_t id) {
  oledShow("ENROLL MODE", "Lift finger,", "then place twice");

  // First image
  oledShow("Place finger", "on sensor...");
  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    oledShow("Image error", "Try again");
    return false;
  }

  oledShow("Lift finger", "");
  while (finger.getImage() != FINGERPRINT_NOFINGER);

  // Second image
  oledShow("Place finger", "again...");
  delay(500);
  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    oledShow("Image error", "Try again");
    return false;
  }

  // Create and store model
  if (finger.createModel() != FINGERPRINT_OK) {
    oledShow("Mismatch!", "Try again");
    return false;
  }
  if (finger.storeModel(id) != FINGERPRINT_OK) {
    oledShow("Store failed!");
    return false;
  }

  oledShow("Enrolled!", ("ID: " + String(id)).c_str());
  Serial.print("[FP] Enrolled ID: ");
  Serial.println(id);
  delay(2000);
  return true;
}

uint8_t getNextFreeID() {
  for (uint8_t i = 1; i < 128; i++) {
    if (finger.loadModel(i) != FINGERPRINT_OK) return i;
  }
  return 0;
}

void adminEnrollmentMode() {
  oledShow("ADMIN MODE", "Press ENROLL", "to add finger");
  Serial.println("[ADMIN] Enrollment mode active.");

  unsigned long timeout = millis() + 15000;

  while (millis() < timeout) {
    if (digitalRead(BTN_ENROLL) == LOW) {
      delay(50);
      uint8_t id = getNextFreeID();
      if (id == 0) {
        oledShow("Memory full!");
        return;
      }
      enrollFingerprint(id);
      timeout = millis() + 15000;
    }
    if (digitalRead(BTN_ADMIN) == LOW) {
      delay(ENROLL_HOLD_MS);
      if (digitalRead(BTN_ADMIN) == LOW) return;
    }
  }
  oledShow("Admin timeout", "Exiting...");
  delay(1000);
}