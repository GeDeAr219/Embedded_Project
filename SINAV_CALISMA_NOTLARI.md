# 📚 Sınav Çalışma Günlüğü — Biyometrik Kapı Kilidi Projesi

> Bu dosya, Claude ile sınav çalışması sırasında **neler istediğimi** ve
> **nerede kaldığımızı** takip etmek için tutuluyor. Yarın açtığında buradan
> devam et — Claude'a "şu dosyadaki nerede kaldık" diye sormana gerek yok.

**Son güncelleme:** 2026-06-12

---

## 🎯 Amaç
Hoca, bu projedeki kodlardan sınavda soru soracak. Tüm sistemi (Python yüz
tanıma + Arduino Mega + ESP32-CAM) anlayıp soru-cevaba hazır olmak.

---

## ✅ Şimdiye Kadar Claude'dan İstediklerim

1. **"Projeyi incele, sonra konuşalım"**
   → Claude tüm projeyi inceledi, mimariyi çıkardı (3 parça: ESP32-CAM,
     Arduino Mega, PC/Python). Genel resmi ve kritik kavramları özetledi.

2. **"Genel tekrar yapalım"** (Hepsi - komple)
   → Claude soru-cevap formatında baştan sona çalışma rehberi hazırladı:
     - Bölüm 1: Sistem mimarisi
     - Bölüm 2: Python / yüz tanıma (embedding, kosinüs benzerliği, threshold)
     - Bölüm 3: Arduino Mega (state machine, parmak izi, EEPROM log, RTC)
     - Bölüm 4: ESP32-CAM & haberleşme
     - Bölüm 5: Koddaki tutarsızlıklar (hata bul soruları)

3. **"ESP32CAM_Face.ino kodlarını anlat"**
   → Claude dosyayı satır satır anlattı: kütüphaneler, kamera init, PSRAM,
     doFaceCheck(), doFaceRegister(), web sunucusu (/stream, /capture),
     setup(), loop(). Muhtemel sınav sorularıyla bitirdi.

4. **"POST ile web siteye bir şey yolluyoruz, nerede nasıl oluyor?"**
   → Claude POST'un iki ucunu anlattı:
     - GÖNDEREN: ESP32 `http.POST(fb->buf, fb->len)` (ESP32CAM_Face.ino:141)
     - ALAN: Python `do_POST()` → `_handle_check()` (wifi_server.py:55,74)
     - Tüm yolculuk şeması + GET/POST farkı + endpoint mantığı.

---

## 📍 ŞU AN NERDE KALDIK

Son konu: **HTTP POST akışı** (ESP32 → PC arası fotoğraf gönderimi).
Claude şunları sormamı önerdi (DEVAM NOKTASI):
- [ ] `_handle_register` tarafının POST akışı (kayıt nasıl oluyor)
- [ ] Python sunucusunun **thread** olayı (`check_same_thread=False`,
      neden arka planda çalışıyor)

---

## 📂 Proje Dosya Haritası (hızlı hatırlatma)

### Python (PC tarafı — yüz tanıma motoru)
- `main.py` — InsightFace ile yüz tanıma, embedding karşılaştırma, ana giriş
- `database.py` — SQLite (users + access_logs tabloları)
- `wifi_server.py` — ESP32'nin konuştuğu HTTP sunucusu (/check, /register, /users, /logs)
- `enroll.py` — yeni yüz kaydetme aracı
- `view_logs.py` — logları görüntüleme/CSV export
- `test_check.py` — hızlı test aracı

### Arduino Mega (kontrolcü — beyin)
- `BiometricSecurity.ino` — ana kod, state machine, grant/deny access
- `fingerprint.ino` — parmak izi sensörü (enroll, search, delete)
- `oled_display.ino` — OLED ekran fonksiyonları
- `access_log.ino` — EEPROM + RTC ile log tutma
- `config.h` — pin tanımları ve sabitler

### ESP32-CAM (kamera + WiFi köprüsü)
- `ESP32CAM_Face.ino` — kamera, WiFi, UART, HTTP POST

### Diğer
- `FingerprintTest/FingerprintTest.ino` — bağımsız parmak izi test menüsü

---

## 🔑 Akılda Kalması Gereken Kritik Kavramlar

| Kavram | Nerede | Özet |
|--------|--------|------|
| Embedding & kosinüs benzerliği | main.py:61-68 | 512D vektör, normalize, 1-dot, threshold 0.60 |
| 2 faktörlü kimlik (2FA) | Mega handleCamMessage | Yüz + parmak izi ikisi de şart |
| State machine | BiometricSecurity.ino:3 | IDLE/GRANTED/DENIED/ADMIN |
| EEPROM circular buffer | access_log.ino:35-41 | 100 kayıt dolunca başa döner |
| UART 9600 baud | ESP↔Mega | Ucuz level shifter 115200'ü bozuyor |
| Level shifter / 3.3V-5V | ESP yorumları | ESP pinleri 5V toleranslı değil |
| HTTP POST (fotoğraf) | ESP→PC | http.POST(buf,len) → do_POST() → imdecode |
| PSRAM | initCamera | Varsa VGA, yoksa QVGA çözünürlük |

---

## ⚠️ Koddaki Tutarsızlıklar (hoca "hata bul" diye sorabilir)

1. **Port uyuşmazlığı:** ESP `8080`'e POST atıyor (ESP32CAM_Face.ino:44),
   Python `5000`'de dinliyor (main.py:174). enroll.py de `5000` kullanıyor.
   IP'ler de farklı (.2 / .3 / .4).
2. **Mega FACE_OK:id'yi kullanmıyor:** ESP `FACE_OK:<id>` yolluyor ama Mega
   sadece `msg == "FACE_OK"` kontrol ediyor, ID karşılaştırması yapmıyor →
   2FA eşleşmesi tam kapanmıyor.
3. **PING kontrolü yanlış yerde:** ESP32CAM_Face.ino:326, mantık hatalı.

---

## 💬 Yarın Claude'a Nasıl Devam Ettiririm?

Şunu yaz:
> "Kanka SINAV_CALISMA_NOTLARI.md dosyasını oku, nerede kaldıysak oradan devam edelim"

Veya direkt yeni konu söyle:
> "Kanka [dosya adı] / [konu] anlat" — örn. "database.py anlat",
> "state machine'i anlat", "parmak izi enroll'u anlat"
