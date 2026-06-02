#include <EEPROM.h>
#include <RTClib.h>
#include "config.h"

RTC_DS3231 rtc;

struct LogEntry {
  uint32_t unixTime;  // timestamp (4 bytes)
  uint8_t  userID;    // fingerprint ID (1 byte)
  uint8_t  result;    // 1=granted, 0=denied (1 byte)
  uint8_t  reason;    // 0=face fail, 1=fp fail, 2=success (1 byte)
  uint8_t  reserved;  // padding (1 byte) → total 8 bytes
};



void logInit() {
  if (!rtc.begin()) {
    Serial.println("[LOG] RTC not found!");
  }
  // Uncomment the line below ONCE to set the time, then comment it again
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void writeLog(uint8_t userID, uint8_t result, uint8_t reason) {
  DateTime now = rtc.now();

  LogEntry entry;
  entry.unixTime = now.unixtime();
  entry.userID   = userID;
  entry.result   = result;
  entry.reason   = reason;
  entry.reserved = 0;

  uint8_t count = EEPROM.read(EEPROM_LOG_COUNT);
  if (count > MAX_LOG_ENTRIES) count = 0;
  if (count >= MAX_LOG_ENTRIES) count = 0;  // circular buffer

  int addr = EEPROM_LOG_START + count * sizeof(LogEntry);
  EEPROM.put(addr, entry);
  EEPROM.write(EEPROM_LOG_COUNT, count + 1);

  Serial.print("[LOG] Written at slot ");
  Serial.println(count);
}

void printAllLogs() {
  uint8_t count = EEPROM.read(EEPROM_LOG_COUNT);
  if (count > MAX_LOG_ENTRIES) count = 0;

  Serial.println("========== ACCESS LOG ==========");
  Serial.print("Total entries: ");
  Serial.println(count);
  Serial.println("--------------------------------");

  for (int i = 0; i < count; i++) {
    LogEntry e;
    EEPROM.get(EEPROM_LOG_START + i * sizeof(LogEntry), e);

    DateTime t(e.unixTime);
    char timeStr[20];
    sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
            t.year(), t.month(),  t.day(),
            t.hour(), t.minute(), t.second());

    const char* resultStr = (e.result == 1) ? "GRANTED" : "DENIED";
    const char* reasonStr;
    switch (e.reason) {
      case REASON_FACE_FAIL: reasonStr = "Face fail"; break;
      case REASON_FP_FAIL:   reasonStr = "FP fail";   break;
      case REASON_SUCCESS:   reasonStr = "Success";   break;
      default:               reasonStr = "Unknown";
    }

    Serial.print("[");
    Serial.print(i);
    Serial.print("] ");
    Serial.print(timeStr);
    Serial.print(" | User:");
    Serial.print(e.userID);
    Serial.print(" | ");
    Serial.print(resultStr);
    Serial.print(" | ");
    Serial.println(reasonStr);
  }
  Serial.println("================================");
}

void clearAllLogs() {
  EEPROM.write(EEPROM_LOG_COUNT, 0);
  Serial.println("[LOG] All logs cleared.");
}