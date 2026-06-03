/*
 * ============================================================
 *  CMP3010 - Biometric Auth System
 *  AŞAMA 2: Parmak İzi Sensörü Test Kodu (Menülü)
 *  Sensör: AS608 / R307  --  Arduino Mega Serial1
 * ============================================================
 *
 *  KURULUM:
 *  1. Kütüphane kur: "Adafruit Fingerprint Sensor Library"
 *     (Library Manager > "Adafruit Fingerprint" ara)
 *  2. Kodu yükle, Serial Monitor 115200 baud.
 *
 *  BAĞLANTI (Serial1):
 *     Sensör VCC (kirmizi) -> 5V
 *     Sensör GND (siyah)   -> GNDa
 *     Sensör TX            -> Mega RX1 (pin 19)
 *     Sensör RX            -> Mega TX1 (pin 18)
 *     (TX/RX caprazlama olur. Renkler modele gore degisir.)
 *
 *  NOT: Cogu AS608/R307 fabrika baud'u 57600'dur.
 *       Baglanmazsa asagidaki FINGER_BAUD'u 9600 dene.
 * ============================================================
 */

#include <Adafruit_Fingerprint.h>

#define FINGER_BAUD 57600   // Baglanmazsa 9600 dene

// Mega'da Serial1 = pin 18 (TX1), pin 19 (RX1)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial1);

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  delay(200);
  Serial.println(F("\n\n=== PARMAK IZI SENSOR TESTI ==="));

  finger.begin(FINGER_BAUD);
  delay(100);

  if (finger.verifyPassword()) {
    Serial.println(F("[OK] Sensor bulundu ve baglandi."));
  } else {
    Serial.println(F("[HATA] Sensor bulunamadi!"));
    Serial.println(F("  - Kablo TX/RX capraz mi? (sensor TX -> pin19)"));
    Serial.println(F("  - VCC 5V, GND ortak mi?"));
    Serial.println(F("  - FINGER_BAUD'u 9600 deneyebilirsin."));
    Serial.println(F("  Sistem durdu."));
    while (true) { delay(1000); }
  }

  // Sensor bilgileri
  finger.getParameters();
  Serial.print(F("  Kapasite      : ")); Serial.println(finger.capacity);
  Serial.print(F("  Guvenlik sev. : ")); Serial.println(finger.security_level);

  finger.getTemplateCount();
  Serial.print(F("  Kayitli parmak: ")); Serial.println(finger.templateCount);

  printMenu();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r' || c == ' ') return;

    switch (c) {
      case 'e': case 'E': enrollFlow();   break;
      case 's': case 'S': searchFlow();   break;
      case 'c': case 'C': countFlow();    break;
      case 'd': case 'D': deleteFlow();   break;
      case 'x': case 'X': emptyFlow();    break;
      case 'm': case 'M': printMenu();    return;
      default:
        Serial.print(F("Gecersiz secim: ")); Serial.println(c);
    }
    printMenu();
  }
}

// ============================================================
//  MENU
// ============================================================
void printMenu() {
  Serial.println(F("\n---------- PARMAK IZI MENU ----------"));
  Serial.println(F("E - Parmak Kaydet (Enroll)"));
  Serial.println(F("S - Parmak Dogrula (Search)"));
  Serial.println(F("C - Kayitli parmak sayisi"));
  Serial.println(F("D - Belirli ID sil"));
  Serial.println(F("X - TUM kayitlari sil"));
  Serial.println(F("m - Menuyu goster"));
  Serial.print  (F("Secim > "));
}

// Serial'den sayi oku (Enter'a kadar bekler)
int readNumber() {
  while (true) {
    while (!Serial.available()) { delay(10); }
    int n = Serial.parseInt();
    if (Serial.read() == '\n' || true) {  // satir sonunu temizle
      while (Serial.available()) Serial.read();
    }
    if (n > 0) return n;
    Serial.print(F("Gecerli bir ID gir (1-127): "));
  }
}

// ============================================================
//  C) SAYI
// ============================================================
void countFlow() {
  finger.getTemplateCount();
  Serial.print(F("\n>> Kayitli parmak sayisi: "));
  Serial.println(finger.templateCount);
}

// ============================================================
//  E) ENROLL (KAYIT)
// ============================================================
void enrollFlow() {
  Serial.println(F("\n>> PARMAK KAYDI"));
  Serial.print(F("Hangi ID'ye kaydedilsin? (1-127): "));
  int id = readNumber();
  if (id < 1 || id > 127) { Serial.println(F("Gecersiz ID.")); return; }
  Serial.print(F("ID #")); Serial.print(id); Serial.println(F(" icin kayit basliyor."));

  int p = -1;
  // 1. okuma
  Serial.println(F("Parmagini sensore koy..."));
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) { delay(50); }
    else if (p != FINGERPRINT_OK) { Serial.println(F("  Goruntu hatasi, tekrar dene.")); }
  }
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println(F("  Ozellik cikarilamadi. Iptal.")); return;
  }
  Serial.println(F("  1. okuma OK. Parmagini KALDIR."));
  delay(1500);

  // parmak kalkana kadar bekle
  p = 0;
  while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); }

  // 2. okuma
  Serial.println(F("Ayni parmagi TEKRAR koy..."));
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) { delay(50); }
  }
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println(F("  2. okuma hatasi. Iptal.")); return;
  }

  // model olustur ve kaydet
  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println(F("  Iki okuma uyusmadi! Bastan dene.")); return;
  }
  if (finger.storeModel(id) == FINGERPRINT_OK) {
    Serial.print(F("  [OK] ID #")); Serial.print(id); Serial.println(F(" kaydedildi!"));
  } else {
    Serial.println(F("  [HATA] Kayit basarisiz."));
  }
}

// ============================================================
//  S) SEARCH (DOGRULA)
// ============================================================
void searchFlow() {
  Serial.println(F("\n>> PARMAK DOGRULAMA"));
  Serial.println(F("Parmagini sensore koy..."));

  int p = -1;
  unsigned long t0 = millis();
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      if (millis() - t0 > 8000) { Serial.println(F("  Zaman asimi, iptal.")); return; }
      delay(50);
    }
  }
  if (finger.image2Tz() != FINGERPRINT_OK) {
    Serial.println(F("  Goruntu islenemedi.")); return;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print(F("  [ESLESTI] ID #")); Serial.print(finger.fingerID);
    Serial.print(F("  Guven: ")); Serial.println(finger.confidence);
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println(F("  [RED] Eslesme yok - bilinmeyen parmak."));
  } else {
    Serial.println(F("  Arama hatasi."));
  }
}

// ============================================================
//  D) BELIRLI ID SIL
// ============================================================
void deleteFlow() {
  Serial.print(F("\n>> Silinecek ID (1-127): "));
  int id = readNumber();
  if (finger.deleteModel(id) == FINGERPRINT_OK) {
    Serial.print(F("  [OK] ID #")); Serial.print(id); Serial.println(F(" silindi."));
  } else {
    Serial.println(F("  [HATA] Silinemedi."));
  }
}

// ============================================================
//  X) TUM KAYITLARI SIL
// ============================================================
void emptyFlow() {
  Serial.println(F("\n>> TUM kayitlari silmek icin onayla."));
  Serial.print(F("Eminsen 'y' yaz: "));
  while (!Serial.available()) { delay(10); }
  char c = Serial.read();
  while (Serial.available()) Serial.read();
  if (c == 'y' || c == 'Y') {
    if (finger.emptyDatabase() == FINGERPRINT_OK) {
      Serial.println(F("  [OK] Tum kayitlar silindi."));
    } else {
      Serial.println(F("  [HATA] Silme basarisiz."));
    }
  } else {
    Serial.println(F("  Iptal edildi."));
  }
}
