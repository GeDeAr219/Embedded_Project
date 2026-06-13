# 🎓 BİYOMETRİK KAPI KİLİDİ — SINAV ÇALIŞMA PROMPT'U

> **NASIL KULLANILIR (sana not — bu kısmı Claude'a yapıştırma):**
> 1. Mobil Claude uygulamasında **yeni bir sohbet** aç.
> 2. Aşağıdaki `═══ KOPYALA ═══` çizgisinden **dosyanın sonuna kadar** olan her şeyi kopyala-yapıştır.
>    (İçinde hem öğretmen talimatı hem de TÜM kodlar gömülü — başka bir şey yapıştırmana gerek yok.)
> 3. Claude "Hazırım, hangi dosyadan başlayalım?" diyecek. Sen "main.py anlat" / "state machine anlat" gibi söyle.
> 4. Mobilde tek mesaj çok uzun gelirse: önce talimat kısmını (KODLAR başlığına kadar), ardından "KODLAR" bölümünü ayrı mesajda yapıştır.
> 5. Çalışırken bir konu kafana yatmazsa "dur, şunu tekrar anlat" / "örnek ver" / "beni sınava çek" de.

---

═══════════════════════════════ KOPYALA ═══════════════════════════════

# ROLÜN

Sen benim **gömülü sistemler ve yazılım hocamsın**. Yarın/yakında bir sınavım var ve hocam aşağıda kodlarını verdiğim **biyometrik kapı kilidi projesinden** soru soracak. Senin görevin bu projedeki **her dosyayı tek tek, sıfırdan, ders anlatır gibi** bana öğretmek. Acelen yok, ben anlayana kadar buradasın.

Üslup: **Türkçe, samimi ama disiplinli bir hoca** gibi. Bana "kanka/dostum" diye hitap edebilirsin ama anlatım net ve öğretici olsun. Terimleri **basit dille** açıkla, gerektiğinde **günlük hayattan analoji** ver, **basitten zora** git. Beni ezberletme, **mantığı kavrat**.

Çok önemli kural: **Tek seferde SADECE BİR dosya/konu** anlat. Dosyayı bitirince DUR, beni yokla, ben "devam" / "sıradaki" demeden öbürüne geçme. Bana bir anda 5 dosya birden anlatma.

---

# PROJE NEDİR? (Genel Bağlam)

Bu bir **2 faktörlü (yüz + parmak izi) biyometrik kapı kilidi sistemi**. Kapıya gelen kişi önce **yüzünü** tanıtır, sonra **parmak izini** bastırır. İkisi de **aynı kişiye** ait olursa kapı açılır; biri tutmazsa veya iki faktör farklı kişiye aitse **alarm** çalar / erişim reddedilir. Her giriş denemesi **loglanır**.

## Sistem 3 ayrı beyinden oluşur (mimari):

```
   ┌──────────────┐   "CHECK"/"REGISTER"    ┌──────────────┐   HTTP POST /check     ┌────────────────┐
   │ ARDUINO MEGA │ ──────UART (9600)─────▶  │  ESP32-CAM   │ ──── (WiFi) ─────────▶ │  PC / PYTHON   │
   │  (BEYİN)     │ ◀──"FACE_OK:id" vb.───── │ (KAMERA+WiFi)│ ◀─── "MATCH:id" ────── │ (YÜZ TANIMA)   │
   └──────────────┘                          └──────────────┘                        └────────────────┘
        │  ▲                                                                                  │
   parmak izi,                                                                          InsightFace AI,
   OLED, röle,                                                                          SQLite veritabanı
   buzzer, LED,                                                                         (embedding'ler)
   buton, EEPROM, RTC
```

1. **ARDUINO MEGA — "Beyin / Kontrolcü"**
   Tüm mantığı yönetir. Butonları okur, parmak izi sensörünü çalıştırır, OLED ekrana yazar, röleyle kapı kilidini açar, buzzer/LED'i sürer, giriş kayıtlarını EEPROM'a yazar (RTC'den zaman alır). Yüz tanıma onun işi DEĞİL — onu ESP32-CAM'e havale eder.

2. **ESP32-CAM — "Köprü / Göz"**
   Kamerası + WiFi'si var. Mega'dan UART ile komut bekler ("CHECK" gelince foto çekip PC'ye yollar). PC'nin cevabını tek kelimeye çevirip ("FACE_OK" gibi) Mega'ya geri verir. Ayrıca `/stream`, `/capture` web sayfaları sunar (kayıt için).

3. **PC / PYTHON — "Yüz Tanıma Motoru"**
   Asıl yapay zekâ burada. ESP32'den gelen JPEG fotoğrafı alır, **InsightFace** ile yüzü **512 boyutlu bir sayı vektörüne (embedding)** çevirir, veritabanındaki kayıtlı yüzlerle **kosinüs benzerliği** ile karşılaştırır, MATCH/NO_MATCH/NO_FACE kararını döner. Kayıtları **SQLite** veritabanında tutar, bir **HTTP sunucusu** ile ESP32'yle konuşur.

## Donanım listesi
- **Arduino Mega 2560** (ana kontrolcü)
- **ESP32-CAM (AI-Thinker)** (kamera + WiFi köprüsü)
- **AS608 / R307 parmak izi sensörü** (Mega'da Serial1, 57600 baud)
- **SSD1306 128x64 OLED ekran** (I2C, adres 0x3C)
- **DS3231 RTC** (gerçek zaman saati — log'lara tarih/saat basmak için)
- **Röle** (kapı kilidini açar — bazı kodda active-LOW)
- **Buzzer** (sesli uyarı/alarm)
- **Yeşil/Kırmızı LED** (granted/denied göstergesi)
- **Butonlar** (login, enroll, silence, cancel, reset vb.)
- **Level shifter / gerilim bölücü** (Mega 5V TX → ESP 3.3V RX için ŞART)
- **PC** (Python + InsightFace yüz tanıma motorunu çalıştırır)

## İki ana senaryo (veri akışı)

**A) GİRİŞ (Login / 2 faktörlü doğrulama):**
1. Kullanıcı Mega'da "Login" butonuna basar.
2. Mega, ESP32'ye UART'tan `CHECK\n` yollar.
3. ESP32 kamerayla foto çeker, PC'ye `POST /check` ile JPEG'i gönderir.
4. PC: JPEG'i çözer → InsightFace ile embedding çıkarır → kayıtlılarla karşılaştırır → `MATCH:<id>` / `NO_MATCH` / `NO_FACE` döner. Sonucu veritabanına loglar.
5. ESP32 cevabı tek kelimeye çevirir: `FACE_OK:<id>` / `FACE_UNKNOWN` / `FACE_NONE` / `FACE_ERR` → Mega'ya yollar.
6. Yüz tanındıysa Mega **parmak izi** ister, sensörde arama yapar (`fingerSearch`).
7. **Yüzün verdiği ID == parmağın verdiği ID** ise → `grantAccess()` (kapı açılır). Farklıysa veya bilinmiyorsa → alarm/deny.

**B) KAYIT (Enroll — yeni kullanıcı ekleme):**
1. Mega'da "Enroll" butonu → yeni `id = templateCount + 1`.
2. Mega ESP32'ye `REGISTER:<id>` yollar.
3. ESP32 foto çekip PC'ye `POST /register?name=<id>` atar.
4. PC: embedding çıkarır → SQLite'a `add_user(id, embedding)` ile kaydeder → canlı listeyi günceller (yeniden başlatma gerekmez).
5. ESP32 `REG_OK` döner, Mega aynı `id` ile **parmak izini** de kaydeder (2 okuma → `createModel` → `storeModel`). Böylece yüz ID'si ile parmak ID'si aynı kişide eşleşir.

---

# DOSYA HARİTASI (neyin nerede olduğu)

### 🐍 PYTHON (PC tarafı — yüz tanıma motoru)
| Dosya | Görevi |
|-------|--------|
| `main.py` | Ana giriş noktası. InsightFace ile yüz tanıma, embedding karşılaştırma, MATCH kararı, seri port dinleyici, WiFi sunucusunu başlatma. |
| `database.py` | SQLite veritabanı katmanı. `users` (isim+embedding) ve `access_logs` tabloları, ekle/sil/listele/log/reset. |
| `wifi_server.py` | ESP32'nin konuştuğu HTTP sunucusu. `/check`, `/register`, `/users`, `/logs`, `/ping`, `/reset` endpoint'leri. |
| `enroll.py` | Yeni yüz kaydetme aracı (çalışan sunucunun `/register`'ına gider). |
| `view_logs.py` | Giriş loglarını terminalden görüntüleme + CSV export. |
| `test_check.py` | Hızlı uçtan uca test (ESP'den 1 kare al → `/check`'e yolla). |
| `requirements.txt` | Python bağımlılıkları. |

### 🔧 ARDUINO MEGA (kontrolcü — beyin)
| Dosya | Görevi |
|-------|--------|
| `BiometricSecurity/BiometricSecurity.ino` | Ana akış (state machine, grantAccess/denyAccess, ESP mesajını işleme). |
| `BiometricSecurity/fingerprint.ino` | Parmak izi menüsü (enroll/search/count/delete). **DİKKAT: kendi setup/loop'u var — bağımsız test kodu.** |
| `BiometricSecurity/oled_display.ino` | OLED ekran yardımcı fonksiyonları. |
| `BiometricSecurity/access_log.ino` | EEPROM + RTC ile giriş logu tutma. |
| `BiometricSecurity/config.h` | Pin tanımları, sabitler, fonksiyon prototipleri. |
| `FingerprintTest/FingerprintTest.ino` | **Asıl tam çalışan 2FA Mega kodu** (buton+OLED+yüz+parmak izi birlikte). |

### 📷 ESP32-CAM (kamera + WiFi köprüsü)
| Dosya | Görevi |
|-------|--------|
| `ESP32CAM_Face/ESP32CAM_Face.ino` | Kamera init, WiFi, UART, HTTP POST, `/stream` ve `/capture` web sunucusu. |

---

# HER DOSYAYI NASIL ANLATACAKSIN (ZORUNLU ŞABLON)

Ben "X dosyasını anlat" dediğimde, **şu 10 başlığı sırayla** uygula. Her başlığı atlamadan, ama gereksiz uzatmadan:

1. **🎯 Tek cümle özet** — Bu dosya kısaca ne yapar?
2. **🧩 Sistemdeki yeri** — 3 parçadan (Mega / ESP32 / PC) hangisine ait, kiminle konuşur, neden var?
3. **📦 Kullanılan kütüphaneler & teknolojiler** — `import`/`#include`'lar tek tek ne işe yarıyor?
4. **🗂️ Dosyanın iç düzeni (yapı)** — Dosya yukarıdan aşağı nasıl dizilmiş? (sabitler → global'ler → fonksiyonlar → setup/loop / main). Hangi fonksiyon nerede, hangi sırada? Bir "iskelet/harita" çıkar.
5. **🔬 Kod açıklaması (blok blok / satır satır)** — Önemli blokları tek tek aç. **Nasıl yazıldığını** anlat: hangi dil yapısı (döngü, koşul, sınıf, struct, enum, pointer vs.) niçin kullanılmış? Kritik satırlarda "bu satır aslında şunu yapıyor" de.
6. **⚙️ Nasıl çalışır (runtime / veri akışı)** — Program çalışınca adım adım ne olur? Veri nereden girer, nasıl işlenir, nereye çıkar? Bir senaryo üzerinden akışı izle.
7. **🔑 Geçen kritik kavramlar** — Bu dosyada karşılaşılan teknik terimleri (embedding, kosinüs benzerliği, UART, baud, EEPROM, struct, thread, BLOB, PSRAM, INPUT_PULLUP, vs.) **mini sözlük** gibi açıkla.
8. **🔗 Diğer dosyalarla ilişkisi** — Hangi dosyayı çağırır, hangi dosya bunu çağırır? Hangi mesajı/veriyi alışveriş eder?
9. **⚠️ Tuzaklar / tutarsızlıklar** — Bu dosyada dikkat edilecek hata, çelişki veya "hoca buradan yakalar" noktası var mı? (Aşağıdaki bilinen tutarsızlıklara da bak.)
10. **📝 Muhtemel sınav soruları + cevapları** — Bu dosyadan 3-5 olası soru sor ve **kısa, net cevaplarını** da ver. Sonunda bana "İstersen seni bu dosyadan sözlü sınava çekeyim mi?" diye sor.

> Şablonu uyguladıktan sonra **DUR**. "Anladın mı? Takıldığın yer var mı? Sıradaki dosyaya geçelim mi?" diye sor. Ben yönlendirene kadar devam etme.

---

# ÜSLUP VE ANLATIM KURALLARI
- **Türkçe** anlat. Terimlerin İngilizcesini parantezde ver (örn. "gömülü vektör (embedding)").
- Kod parçası gösterirken **kısa alıntı** yap, sonra altında açıkla. Tüm dosyayı tekrar yapıştırma.
- **Analoji kullan**: örn. embedding'i "yüzün parmak izi gibi sayısal kimliği", threshold'u "ne kadar benzerse 'aynı kişi' diyeceğimiz tolerans".
- Bir kavram ilk geçtiğinde **dur ve açıkla**, "bunu zaten biliyorsun" varsayma.
- Matematik geçerse (kosinüs benzerliği, normalize) **adım adım, sayı örneğiyle** göster.
- Cevapların **terminalde/telefonda okunacak** — sade markdown kullan, aşırı tablo/emoji yığma.

# ETKİLEŞİM KURALLARI
- Aynı anda tek dosya. Ben "devam/sıradaki/şunu anlat" demeden ilerleme.
- Ben "daha basit anlat", "örnek ver", "şu satırı açma", "beni sınava çek", "özet geç" dersem ona göre uyarla.
- Emin olmadığın bir şey olursa uydurma; "kodda şu var, gerisi belirsiz" de.
- Her dosya sonunda **çalışma sırasına göre bir sonrakini öner** ama kararı bana bırak.

# ÖNERİLEN ÇALIŞMA SIRASI (kolaydan-zora, mantık akışına göre)
1. `config.h` (en kolay — sabitler/pinler, sistemi tanıtır)
2. `database.py` (SQLite temel — bağımsız anlaşılır)
3. `main.py` (yüz tanıma kalbi — embedding, kosinüs benzerliği, threshold)
4. `wifi_server.py` (HTTP sunucu — GET/POST, endpoint'ler, thread)
5. `enroll.py` + `test_check.py` + `view_logs.py` (yardımcı araçlar — hızlı geçilir)
6. `ESP32CAM_Face.ino` (kamera, WiFi, UART, HTTP POST — donanım+ağ köprüsü)
7. `FingerprintTest.ino` (tam 2FA Mega kodu — state akışı, parmak izi, OLED, alarm)
8. `BiometricSecurity.ino` + `fingerprint.ino` + `oled_display.ino` + `access_log.ino` (modüler Mega kodu, EEPROM/RTC log, state machine)
9. **Genel tekrar** (tüm sistem soru-cevap + tutarsızlıklar üzerinden "hata bul")

---

# ⚠️ BİLİNEN TUTARSIZLIKLAR (hoca "hatayı bul" diye sorabilir — bunları aklında tut)
1. **Port/IP uyuşmazlığı:** ESP32 `http://172.20.10.2:8080/check` adresine POST atıyor; Python sunucusu ise `5000` portunda dinliyor, `enroll.py`/`test_check.py` ise `172.20.10.4:5000` kullanıyor. IP'ler (.2/.3/.4) ve portlar (8080 vs 5000) tutarsız — gerçekte hepsi aynı olmalı.
2. **İki ayrı Mega kodu var:** `FingerprintTest.ino` tam çalışan 2FA mantığını içerir (yüz ID'si ile parmak ID'sini KARŞILAŞTIRIR). `BiometricSecurity.ino` ise daha eski/eksik: ESP'den gelen `FACE_OK:<id>` mesajını sadece `msg == "FACE_OK"` ile (tam eşitlik) kontrol eder → id'li mesaj gelince eşleşmez, ayrıca ID karşılaştırması yapmaz → 2FA tam kapanmaz.
3. **`fingerprint.ino` çakışması:** Bu dosya `BiometricSecurity` klasöründe ama **kendi `setup()` ve `loop()` fonksiyonları var**. Arduino bir klasördeki tüm `.ino`'ları birleştirir → `BiometricSecurity.ino` ile çift `setup`/`loop` → **derleme hatası**. Ayrıca `BiometricSecurity.ino`, `fingerprintInit()` ve `verifyFingerprint()` çağırır ama `fingerprint.ino` bunları tanımlamaz (onun yerine menü fonksiyonları var).
4. **ESP32 PING kontrolü yanlış yerde:** `ESP32CAM_Face.ino` loop'unda `else if (cmd == "PING")` dalı, karakter biriktirme aşamasında ve yanlış konumda → "PING" düzgün yakalanmaz (mantık hatası).
5. **Röle polaritesi farkı:** `BiometricSecurity.ino`'da röle HIGH=açık gibi kullanılır; `FingerprintTest.ino`'da `HIGH=locked, LOW=unlocked` (active-LOW) — iki kod farklı varsayıyor.

---

# NASIL BAŞLIYORUZ
Bu talimatı ve aşağıdaki tüm kodları okuduğunu bana **kısaca** ("tamam, projeyi ve 3 parçalı mimariyi gördüm") onayla, **uzun bir özet yapma**. Sonra "Önerdiğim sıra şu: config.h → database.py → main.py ... Hangisinden başlayalım?" diye sor ve benim seçmemi bekle.

---

# 📚 PROJE KODLARI (REFERANS — anlatırken bunları kullan)

> Aşağıda projedeki tüm dosyaların kodu var. Ben "X dosyasını anlat" dediğimde ilgili kodu buradan bul ve yukarıdaki 10 başlıklı şablona göre işle.

## ════════ PYTHON TARAFI ════════

### 📄 main.py
```python
import os
import sys
import cv2
import numpy as np
from insightface.app import FaceAnalysis
from database import FaceDatabase 

class SuppressOutput:
    def __enter__(self):
        self._stdout = sys.stdout
        self._stderr = sys.stderr
        sys.stdout = open(os.devnull, 'w')
        sys.stderr = open(os.devnull, 'w')

    def __exit__(self, exc_type, exc_val, exc_tb):
        sys.stdout.close()
        sys.stderr.close()
        sys.stdout = self._stdout
        sys.stderr = self._stderr

class FaceRecognizer:
    def __init__(self):
        self.reg_names = []
        self.reg_matrix = np.empty((0, 512))
        self.threshold = 0.60 
        self.db = FaceDatabase()

        with SuppressOutput():
            self.app = FaceAnalysis(name='buffalo_s', providers=['CUDAExecutionProvider', 'CPUExecutionProvider'])
            self.app.prepare(ctx_id=0, det_size=(320, 320))

    def get_embedding(self, img_path):
        img = cv2.imread(img_path)
        if img is None:
            raise Exception(f"Could not read image {img_path}")
        faces = self.app.get(img)
        if not faces:
            raise ValueError("No face detected.")
        return faces[0].embedding

    def load_registered_faces(self):
        # Database'deki tüm kullanıcıları yükler
        # İsimleri ve embeddingleri RAM'e alır
        self.reg_names, raw_matrix = self.db.load_users()
        # Eğer kayıtlı kullanıcı varsa embeddingleri normalize edip matrise dönüştürür
        if len(self.reg_names) > 0:
            self.reg_matrix = raw_matrix / np.linalg.norm(raw_matrix, axis=1, keepdims=True)
        # Eğer database boşsa boş embedding matrisi oluştur
        else:
            self.reg_matrix = np.empty((0, 512))

    def verify_in_memory(self, img):
        faces = self.app.get(img)
        if not faces:
            return ("NO_FACE", None, None)
            
        input_emb = faces[0].embedding
        if len(self.reg_names) == 0:
            return ("NO_MATCH", None, None)
            
        input_emb = input_emb / np.linalg.norm(input_emb)
        similarities = np.dot(self.reg_matrix, input_emb)
        distances = 1.0 - similarities
        best_idx = np.argmin(distances)
        min_dist = distances[best_idx]
        best_match = self.reg_names[best_idx]
        
        if min_dist <= self.threshold:
            return ("MATCH", best_match, min_dist)
        return ("NO_MATCH", best_match, min_dist)

    def verify_image(self, input_img_path):
        img = cv2.imread(input_img_path)
        if img is None:
             return
             
        result = self.verify_in_memory(img)
        if result[0] == "MATCH":
            print(f"[MATCH] {os.path.basename(input_img_path)} -> {result[1]} (dist: {result[2]:.4f})")
        elif result[0] == "NO_MATCH":
            print(f"[NO_MATCH] {os.path.basename(input_img_path)}")
        else:
            print(f"[NO_FACE] {os.path.basename(input_img_path)}")

    def register_face(self, img, name):
         # Görüntüdeki yüzleri algılar ve ilk yüzün embeddingini alır, ardından database'e kaydeder
        faces = self.app.get(img)
        if not faces:
            raise ValueError("No face detected.")
        emb = faces[0].embedding
        self.db.add_user(name, np.array(emb))
        # Yeni kullanıcı eklendiği için RAM'deki kullanıcı listesini günceller
        self.load_registered_faces()
        return True



def listen_serial(recognizer, port="COM3", baudrate=115200):
    import serial
    try:
        ser = serial.Serial(port, baudrate, timeout=5)
        ser.flushInput()
        print(f"Serial listener active on {port}...")
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            parts = line.split(":")
            if len(parts) >= 2:
                cmd = parts[0]
                is_register = False
                name = None
                size = 0
                if cmd == "REGISTER":
                    if len(parts) >= 3:
                        name = parts[1]
                        size = int(parts[2])
                        is_register = True
                    else:
                        continue
                elif cmd == "CHECK" or cmd == "START":
                    size = int(parts[1])
                else:
                    continue

                try:
                    img_bytes = ser.read(size)
                    if len(img_bytes) != size:
                        ser.write(b"ERROR:INCOMPLETE\n")
                        continue
                    
                    nparr = np.frombuffer(img_bytes, np.uint8)
                    img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                    if img is None:
                        ser.write(b"ERROR:DECODE_FAILED\n")
                        continue
                    
                    if is_register:
                        try:
                            recognizer.register_face(img, name)
                            ser.write(b"REGISTER_OK\n")
                            print(f"REGISTER_OK: {name}")
                        except Exception as e:
                            ser.write(b"ERROR:REGISTER_FAILED\n")
                            print(f"Registration failed: {e}")
                    else:
                        status, match_name, dist = recognizer.verify_in_memory(img)
                        if status == "MATCH":
                            recognizer.db.add_log(match_name, "MATCH", float(dist))
                            ser.write(f"MATCH:{match_name}\n".encode('utf-8'))
                            print(f"MATCH: {match_name} ({dist:.3f})")
                        elif status == "NO_MATCH":
                            recognizer.db.add_log("UNKNOWN", "NO_MATCH", None)
                            ser.write(b"NO_MATCH\n")
                            print("NO_MATCH")
                        else:
                            recognizer.db.add_log("NONE", "NO_FACE", None)
                            ser.write(b"NO_FACE\n")
                            print("NO_FACE")
                except Exception:
                    ser.write(b"ERROR:EXCEPTION\n")
    except Exception as e:
        print(f"Serial error: {e}")

if __name__ == "__main__":
    import sys
    
    mode = "folder"
    if len(sys.argv) > 1 and sys.argv[1].lower() in ["serial", "-s"]:
        mode = "serial"
            
    recognizer = FaceRecognizer()
    recognizer.load_registered_faces()

    from wifi_server import start_wifi_server
    start_wifi_server(recognizer, port=5000)
    
    if mode == "serial":
        port = sys.argv[2] if len(sys.argv) > 2 else "COM3"
        listen_serial(recognizer, port=port)
    else:
        import time
        INPUT_DIR = "input"
        os.makedirs(INPUT_DIR, exist_ok=True)
        files = [f for f in os.listdir(INPUT_DIR) if f.lower().endswith(('.png', '.jpg', '.jpeg'))]
        for file in files:
            start_time = time.time()
            recognizer.verify_image(os.path.join(INPUT_DIR, file))
            print(f"Time: {(time.time() - start_time)*1000:.1f}ms")
     
        # ── YENİ: WiFi server'ı canlı tut ──────────────────
        print("[SYS] WiFi server running. Press Ctrl+C to stop.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("[SYS] Shutting down.")
        # ────────────────────────────────────────────────────
```

### 📄 database.py
```python
import sqlite3
import numpy as np


class FaceDatabase:
    def __init__(self, db_path="faces.db"):
        # check_same_thread=False: the WiFi server handles requests in a
        # separate thread from where this connection is created.
        self.conn = sqlite3.connect(db_path, check_same_thread=False)
        self.create_tables()

    def create_tables(self):
        cursor = self.conn.cursor()

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            embedding BLOB NOT NULL
        )
        """)

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS access_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_name TEXT,
            status TEXT NOT NULL,
            distance REAL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
        """)

        self.conn.commit()

    def add_user(self, name, embedding):
        embedding_blob = embedding.astype(np.float32).tobytes()

        cursor = self.conn.cursor()
        cursor.execute(
            "INSERT INTO users (name, embedding) VALUES (?, ?)",
            (name, embedding_blob)
        )
        self.conn.commit()

    def load_users(self):
        cursor = self.conn.cursor()
        cursor.execute("SELECT name, embedding FROM users")
        rows = cursor.fetchall()

        names = []
        embeddings = []

        for name, embedding_blob in rows:
            embedding = np.frombuffer(embedding_blob, dtype=np.float32)
            names.append(name)
            embeddings.append(embedding)

        if len(embeddings) == 0:
            return [], np.empty((0, 512))

        return names, np.array(embeddings)

    def add_log(self, user_name, status, distance=None):
        cursor = self.conn.cursor()
        cursor.execute(
            "INSERT INTO access_logs (user_name, status, distance) VALUES (?, ?, ?)",
            (user_name, status, distance)
        )
        self.conn.commit()
    
    def get_logs(self, limit=50):
        # Fetch access logs ordered by most recent
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT id, user_name, status, distance, created_at
            FROM access_logs
            ORDER BY created_at DESC
            LIMIT ?
        """, (limit,))
        return cursor.fetchall()

    def list_users(self):
        # Return all registered users
        cursor = self.conn.cursor()
        cursor.execute("SELECT id, name FROM users ORDER BY id")
        return cursor.fetchall()

    def delete_user(self, user_id):
        # Delete a user by ID, returns True if deleted
        cursor = self.conn.cursor()
        cursor.execute("DELETE FROM users WHERE id = ?", (user_id,))
        self.conn.commit()
        return cursor.rowcount > 0

    def reset_database(self):
        # Clear both users and access logs, and reset autoincrement ids
        cursor = self.conn.cursor()
        cursor.execute("DELETE FROM users")
        cursor.execute("DELETE FROM sqlite_sequence WHERE name='users'")
        cursor.execute("DELETE FROM access_logs")
        cursor.execute("DELETE FROM sqlite_sequence WHERE name='access_logs'")
        self.conn.commit()
```

### 📄 wifi_server.py
```python
import cv2
import numpy as np
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler

# Global recognizer — set from main.py
_recognizer = None

def set_recognizer(recognizer):
    global _recognizer
    _recognizer = recognizer


class FaceHandler(BaseHTTPRequestHandler):
    """
    HTTP endpoints for ESP32-CAM communication.

    GET  /ping              → check if server is alive
    GET  /users             → list registered users
    GET  /logs              → get recent access logs
    POST /check             → send JPEG, returns MATCH/NO_MATCH/NO_FACE
    POST /register?name=X   → send JPEG + name, registers face
    """

    def log_message(self, format, *args):
        # suppress default HTTP request logs
        pass

    def do_GET(self):
        if self.path == "/ping":
            self._send_text(200, "pong")

        elif self.path == "/users":
            users = _recognizer.db.list_users()
            lines = [f"{uid}:{name}" for uid, name in users]
            self._send_text(200, "\n".join(lines) if lines else "EMPTY")

        elif self.path.startswith("/logs"):
            limit = 50
            if "n=" in self.path:
                try:
                    limit = int(self.path.split("n=")[1])
                except:
                    pass
            logs = _recognizer.db.get_logs(limit)
            lines = []
            for log_id, user, status, dist, ts in logs:
                dist_str = f"{dist:.4f}" if dist is not None else "-"
                lines.append(f"{log_id}|{ts}|{user}|{status}|{dist_str}")
            self._send_text(200, "\n".join(lines) if lines else "EMPTY")

        else:
            self._send_text(404, "NOT_FOUND")

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        if self.path == "/check":
            self._handle_check(body)

        elif self.path.startswith("/register"):
            name = None
            if "name=" in self.path:
                name = self.path.split("name=")[1].split("&")[0]
            self._handle_register(body, name)

        elif self.path == "/reset":
            self._handle_reset()

        else:
            self._send_text(404, "NOT_FOUND")

    def _handle_check(self, img_bytes):
        if _recognizer is None:
            self._send_text(500, "ERROR:NOT_READY")
            return

        nparr = np.frombuffer(img_bytes, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

        if img is None:
            self._send_text(400, "ERROR:DECODE_FAILED")
            return

        try:
            status, match_name, dist = _recognizer.verify_in_memory(img)

            if status == "MATCH":
                _recognizer.db.add_log(match_name, "MATCH", float(dist))
                self._send_text(200, f"MATCH:{match_name}")
                print(f"[WiFi] MATCH: {match_name} ({dist:.3f})")
            elif status == "NO_MATCH":
                _recognizer.db.add_log("UNKNOWN", "NO_MATCH", None)
                self._send_text(200, "NO_MATCH")
                print("[WiFi] NO_MATCH")
            else:
                _recognizer.db.add_log("NONE", "NO_FACE", None)
                self._send_text(200, "NO_FACE")
                print("[WiFi] NO_FACE")
        except Exception as e:
            import traceback
            traceback.print_exc()
            self._send_text(500, f"ERROR:{e}")

    def _handle_register(self, img_bytes, name):
        if _recognizer is None:
            self._send_text(500, "ERROR:NOT_READY")
            return
        if not name:
            self._send_text(400, "ERROR:NO_NAME")
            return

        nparr = np.frombuffer(img_bytes, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

        if img is None:
            self._send_text(400, "ERROR:DECODE_FAILED")
            return

        try:
            _recognizer.register_face(img, name)
            self._send_text(200, f"REGISTER_OK:{name}")
            print(f"[WiFi] REGISTER_OK: {name}")
        except Exception as e:
            self._send_text(500, f"ERROR:{e}")

    def _handle_reset(self):
        if _recognizer is None:
            self._send_text(500, "ERROR:NOT_READY")
            return
        try:
            _recognizer.db.reset_database()
            _recognizer.load_registered_faces()
            self._send_text(200, "RESET_OK")
            print("[WiFi] Database reset successful")
        except Exception as e:
            self._send_text(500, f"ERROR:{e}")

    def _send_text(self, code, text):
        body = text.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)


def start_wifi_server(recognizer, host="0.0.0.0", port=5000):
    """Start WiFi HTTP server in a background thread."""
    set_recognizer(recognizer)
    server = HTTPServer((host, port), FaceHandler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    print(f"[WiFi] Server started → http://{host}:{port}")
    print(f"[WiFi] Endpoints: /check  /register?name=X  /users  /logs  /ping")
    return server
```

### 📄 enroll.py
```python
"""
Enroll a face through the RUNNING face server (main.py).

Registration goes through the live server's /register endpoint, which
updates the in-memory face list immediately — so you NEVER need to
restart main.py after enrolling.

The user id is just a number that must match the fingerprint id enrolled
on the Mega (true same-person 2-factor).

Usage:
  python enroll.py 1            # capture from ESP32-CAM, register user "1"
  python enroll.py 1 photo.jpg  # register user "1" from an image file
  python enroll.py --users      # list registered users
"""
import sys
import time
import urllib.request
import urllib.error

SERVER     = "http://172.20.10.4:5000"     # PC running main.py
ESP_STREAM = "http://172.20.10.3/stream"   # ESP32-CAM MJPEG stream


def _post_register(name, jpg_bytes):
    url = f"{SERVER}/register?name={name}"
    req = urllib.request.Request(url, data=jpg_bytes,
                                 headers={"Content-Type": "image/jpeg"})
    try:
        return urllib.request.urlopen(req, timeout=20).read().decode("utf-8", "ignore")
    except urllib.error.HTTPError as ex:
        return ex.read().decode("utf-8", "ignore")


def enroll_from_esp32(name, stream_url=ESP_STREAM, warmup=4.0, max_attempts=40):
    print(f"[ENROLL] Enrolling '{name}' via {SERVER}")
    print(f"[ENROLL] Look at the camera. Capturing in {warmup:.0f}s, hold still...")
    stream = urllib.request.urlopen(stream_url, timeout=10)
    buf = b""
    t0 = time.time()
    attempts = 0
    while True:
        buf += stream.read(4096)
        s = buf.find(b'\xff\xd8')
        e = buf.find(b'\xff\xd9')
        if s != -1 and e != -1:
            jpg = buf[s:e + 2]
            buf = buf[e + 2:]
            if time.time() - t0 < warmup:   # give the user time to pose
                continue
            resp = _post_register(name, jpg)
            if resp.startswith("REGISTER_OK"):
                print(f"[ENROLL] Success — {resp}")
                print("[ENROLL] Server updated live. No restart needed.")
                return True
            attempts += 1
            print(f"[ENROLL] {resp} — retrying ({attempts}/{max_attempts})")
            if attempts >= max_attempts:
                print("[ENROLL] Gave up — face the camera and retry.")
                return False


def enroll_from_image(name, path):
    with open(path, "rb") as f:
        jpg = f.read()
    resp = _post_register(name, jpg)
    print(f"[ENROLL] {resp}")
    return resp.startswith("REGISTER_OK")


def list_users():
    data = urllib.request.urlopen(f"{SERVER}/users", timeout=10).read().decode("utf-8", "ignore")
    print("---- Registered users ----")
    print(data if data.strip() else "(empty)")


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args or args[0] in ("--users", "-u"):
        list_users()
    elif len(args) == 1:
        enroll_from_esp32(args[0])
    else:
        enroll_from_image(args[0], args[1])
```

### 📄 view_logs.py
```python
import sys
import csv
from datetime import datetime
from database import FaceDatabase


def print_logs(logs):
    if not logs:
        print("No logs found.")
        return

    print(f"\n{'ID':>4} | {'Time':<20} | {'User':<15} | {'Status':<10} | {'Distance'}")
    print("-" * 68)

    for row in logs:
        log_id, user_name, status, distance, created_at = row
        user_str    = (user_name or "-")[:15]
        dist_str    = f"{distance:.4f}" if distance is not None else "-"
        status_icon = "OK" if status == "MATCH" else "!!"
        print(f"{log_id:>4} | {created_at:<20} | {user_str:<15} | "
              f"{status_icon} {status:<8} | {dist_str}")

    print(f"\nTotal: {len(logs)} entries\n")


def export_csv(db, filename="access_log_export.csv"):
    logs = db.get_logs(limit=10000)
    with open(filename, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["ID", "User", "Status", "Distance", "Timestamp"])
        writer.writerows(logs)
    print(f"[EXPORT] Saved {len(logs)} rows to '{filename}'")


def filter_by_status(db, status):
    all_logs = db.get_logs(limit=10000)
    return [row for row in all_logs if row[2] == status]


def interactive_menu(db):
    while True:
        print("\n==== ACCESS LOG VIEWER ====")
        print("  1. Show last 20 entries")
        print("  2. Show last 100 entries")
        print("  3. Show MATCH entries only")
        print("  4. Show DENIED / NO_MATCH entries only")
        print("  5. Export all to CSV")
        print("  6. Exit")
        print("===========================")
        choice = input("Select: ").strip()

        if choice == "1":
            print_logs(db.get_logs(20))
        elif choice == "2":
            print_logs(db.get_logs(100))
        elif choice == "3":
            print_logs(filter_by_status(db, "MATCH"))
        elif choice == "4":
            no_match = filter_by_status(db, "NO_MATCH")
            no_face  = filter_by_status(db, "NO_FACE")
            print_logs(no_match + no_face)
        elif choice == "5":
            ts = datetime.now().strftime("%Y%m%d_%H%M%S")
            export_csv(db, f"log_{ts}.csv")
        elif choice == "6":
            break
        else:
            print("Invalid choice.")


if __name__ == "__main__":
    db = FaceDatabase()

    # show last 50
    if len(sys.argv) == 2 and sys.argv[1].isdigit():
        print_logs(db.get_logs(int(sys.argv[1])))

    elif len(sys.argv) == 2 and sys.argv[1] == "export":
        export_csv(db)

    else:
        interactive_menu(db)
```

### 📄 test_check.py
```python
"""
Quick end-to-end face-check test (no Mega needed).
Grabs a single still from the ESP32-CAM and POSTs it to the PC face
server's /check endpoint, then prints the result.

Run main.py first (so the server is up with enrolled faces loaded).
Usage:  python test_check.py
"""
import urllib.request

ESP_CAPTURE = "http://172.20.10.3/capture"      # ESP32-CAM single still
SERVER_CHECK = "http://172.20.10.4:5000/check"  # PC face server

print(f"[TEST] Grabbing frame from {ESP_CAPTURE} ...")
img = urllib.request.urlopen(ESP_CAPTURE, timeout=10).read()
print(f"[TEST] Got {len(img)} bytes. Sending to {SERVER_CHECK} ...")

req = urllib.request.Request(SERVER_CHECK, data=img,
                             headers={"Content-Type": "image/jpeg"})
resp = urllib.request.urlopen(req, timeout=20).read().decode("utf-8", "ignore")
print(f"[TEST] Server replied: {resp}")
```

### 📄 requirements.txt
```text
insightface
onnxruntime-gpu
opencv-python
numpy
pyserial
```

## ════════ ARDUINO MEGA (BiometricSecurity) ════════

### 📄 config.h
```cpp
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
```

### 📄 BiometricSecurity.ino
```cpp
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
```

### 📄 fingerprint.ino
> NOT: Bu dosya `BiometricSecurity` klasöründe ama **bağımsız bir parmak izi test menüsü** gibi yazılmış (kendi `setup()`/`loop()`'u var). Bu yüzden `BiometricSecurity.ino` ile aynı anda derlenmez — bunu anlatırken vurgula.
```cpp
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
```

### 📄 oled_display.ino
```cpp
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void oledInit() {
  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.display();
}

void oledShow(const char* line1,
              const char* line2,
              const char* line3) {
  oled.clearDisplay();
  oled.setCursor(0, 0);  oled.println(line1);
  oled.setCursor(0, 22); oled.println(line2);
  oled.setCursor(0, 44); oled.println(line3);
  oled.display();
}

void oledShowLarge(const char* text) {
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(0, 20);
  oled.println(text);
  oled.setTextSize(1);
  oled.display();
}
```

### 📄 access_log.ino
```cpp
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
```

## ════════ ARDUINO MEGA (FingerprintTest — tam 2FA kodu) ════════

### 📄 FingerprintTest.ino
```cpp
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
#define BTN_RESET_DB  46
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
  pinMode(BTN_RESET_DB, INPUT_PULLUP);
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
  if (digitalRead(BTN_RESET_DB) == LOW) {
    delay(50);
    if (digitalRead(BTN_RESET_DB) == LOW) {
        oledShow("Hold 3s to", "reset database...");
        unsigned long t = millis();
        while (digitalRead(BTN_RESET_DB) == LOW) {
            if (millis() - t > 3000) {
                // 3 saniye basılı tutulursa sıfırla
                CAM_SERIAL.println("RESET_DB");
                oledShow("DB Resetting...", "Please wait...");
                delay(2000);
                oledShow("DB Reset!", "Done.");
                delay(2000);
                showWelcome();
                break;
            }
        }
        // 3 saniyeden önce bırakılırsa iptal
        if (millis() - t < 3000) {
            oledShow("Cancelled.");
            delay(1000);
            showWelcome();
        }
        while (digitalRead(BTN_RESET_DB) == LOW) { delay(10); }
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
```

## ════════ ESP32-CAM ════════

### 📄 ESP32CAM_Face.ino
```cpp
/*
 * ============================================================
 *  ESP32-CAM Face Gateway  (AI-Thinker board)
 * ============================================================
 *  ROLE:
 *   - Connects to WiFi.
 *   - Waits for "CHECK" command from Arduino Mega over UART.
 *   - On CHECK: captures a JPEG, POSTs it to the PC face server
 *     (wifi_server.py  ->  POST /check), reads the reply, and
 *     relays a single token back to the Mega:
 *         FACE_OK       (MATCH)
 *         FACE_UNKNOWN  (NO_MATCH  -> Mega raises alarm)
 *         FACE_NONE     (NO_FACE   -> Mega asks to retry)
 *         FACE_ERR      (wifi/server/capture problem)
 *   - Also serves an MJPEG stream at  http://<esp-ip>/stream
 *     so enroll.py can register faces, plus /capture for a
 *     single still and / for a status page.
 *
 *  WIRING TO MEGA (UART):
 *     ESP GPIO14 (TX) ── Mega RX2 (pin 17)      [direct OK]
 *     ESP GPIO13 (RX) ── Mega TX2 (pin 16) via level shifter
 *                        (Mega TX is 5V; ESP pins are NOT 5V
 *                         tolerant — use a 1k/2k divider or
 *                         a logic level converter).
 *     Common GND between ESP32-CAM and Mega.
 *
 *  BOARD SETTINGS (Arduino IDE):
 *     Board: "AI Thinker ESP32-CAM"
 *     Partition: "Huge APP (3MB No OTA)"
 *     PSRAM: Enabled
 *  Flash with an FTDI adapter (GPIO0 -> GND while flashing).
 * ============================================================
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_http_server.h"

// ── EDIT THESE ───────────────────────────────────────────────
const char* WIFI_SSID = "Emir";
const char* WIFI_PASS = "emir5353";
// PC running main.py / wifi_server.py (port 5000):
const char* SERVER_CHECK_URL    = "http://172.20.10.2:8080/check";
const char* SERVER_REGISTER_URL = "http://172.20.10.2:8080/register?name=";
// ─────────────────────────────────────────────────────────────

// UART to Arduino Mega (UART1 remapped to free pins)
#define UART_RX_PIN   13   // <- Mega TX2 (pin16) via level shifter
#define UART_TX_PIN   14   // -> Mega RX2 (pin17)
#define UART_BAUD     9600   // low baud: cheap level shifters mangle 115200
HardwareSerial MegaSerial(1);

// ── AI-Thinker ESP32-CAM pin map ─────────────────────────────
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

httpd_handle_t stream_httpd = NULL;

// ── Camera init ──────────────────────────────────────────────
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;   // 640x480
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 15;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

// ── Send one frame to the PC server, map reply to a token ────
void doFaceCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    MegaSerial.println("FACE_ERR");
    Serial.println("[CHECK] WiFi down -> FACE_ERR");
    return;
  }

  // Drop one frame so we get a fresh image, then grab.
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) { esp_camera_fb_return(fb); fb = NULL; }
  fb = esp_camera_fb_get();
  if (!fb) {
    MegaSerial.println("FACE_ERR");
    Serial.println("[CHECK] capture failed -> FACE_ERR");
    return;
  }

  HTTPClient http;
  http.begin(SERVER_CHECK_URL);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(8000);
  int code = http.POST(fb->buf, fb->len);
  String payload = (code > 0) ? http.getString() : "";
  http.end();
  esp_camera_fb_return(fb);

  Serial.printf("[CHECK] HTTP %d  payload='%s'\n", code, payload.c_str());

  if (code != 200) {
    MegaSerial.println("FACE_ERR");
    return;
  }
  if (payload.startsWith("MATCH")) {
    // payload is "MATCH:<name>"; the name IS the user id number.
    // Relay it so the Mega can check it equals the fingerprint id.
    int colon = payload.indexOf(':');
    String id = (colon >= 0) ? payload.substring(colon + 1) : "0";
    id.trim();
    MegaSerial.println("FACE_OK:" + id);
  } else if (payload.startsWith("NO_MATCH")) {
    MegaSerial.println("FACE_UNKNOWN");
  } else if (payload.startsWith("NO_FACE")) {
    MegaSerial.println("FACE_NONE");
  } else {
    MegaSerial.println("FACE_ERR");
  }
}

// ── Register the current face on the PC server under user <id> ──
// Replies to the Mega: REG_OK / REG_FAIL (no face) / REG_ERR (wifi/server)
void doFaceRegister(const String& id) {
  if (WiFi.status() != WL_CONNECTED) {
    MegaSerial.println("REG_ERR");
    Serial.println("[REG] WiFi down -> REG_ERR");
    return;
  }

  String url = String(SERVER_REGISTER_URL) + id;

  // Try a few frames so the user has a moment to face the camera.
  for (int attempt = 0; attempt < 12; attempt++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) { esp_camera_fb_return(fb); fb = NULL; }   // drop stale frame
    fb = esp_camera_fb_get();
    if (!fb) { delay(200); continue; }

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "image/jpeg");
    http.setTimeout(8000);
    int code = http.POST(fb->buf, fb->len);
    String payload = (code > 0) ? http.getString() : "";
    http.end();
    esp_camera_fb_return(fb);

    Serial.printf("[REG] try %d  HTTP %d  payload='%s'\n",
                  attempt, code, payload.c_str());

    if (code == 200 && payload.startsWith("REGISTER_OK")) {
      MegaSerial.println("REG_OK");
      return;
    }
    // no face yet / transient error → wait and retry
    delay(300);
  }
  MegaSerial.println("REG_FAIL");
  Serial.println("[REG] gave up -> REG_FAIL");
}

// ── HTTP: single still at /capture ───────────────────────────
static esp_err_t capture_handler(httpd_req_t* req) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { httpd_resp_send_500(req); return ESP_FAIL; }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// ── HTTP: MJPEG stream at /stream (used by enroll.py) ────────
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t* req) {
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char part_buf[64];
  while (true) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }

    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;  // client disconnected
  }
  return res;
}

// ── HTTP: status page at / ───────────────────────────────────
static esp_err_t index_handler(httpd_req_t* req) {
  String html = "<html><body style='font-family:sans-serif'>"
                "<h2>ESP32-CAM Face Gateway</h2>"
                "<p><a href='/stream'>/stream</a> (MJPEG, used by enroll.py)</p>"
                "<p><a href='/capture'>/capture</a> (single JPEG)</p>"
                "<img src='/stream' width='320'></body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html.c_str(), html.length());
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port   = 32768;

  httpd_uri_t index_uri   = { "/",        HTTP_GET, index_handler,   NULL };
  httpd_uri_t capture_uri = { "/capture", HTTP_GET, capture_handler, NULL };
  httpd_uri_t stream_uri  = { "/stream",  HTTP_GET, stream_handler,  NULL };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
    httpd_register_uri_handler(stream_httpd, &capture_uri);
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("[HTTP] server started on port 80");
  }
}

// ── Setup / Loop ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  MegaSerial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  if (!initCamera()) {
    Serial.println("[CAM] FATAL — halting.");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] connecting");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] NOT connected — checks will return FACE_ERR.");
  }

  startCameraServer();
  Serial.println("[SYS] Ready. Waiting for CHECK / REGISTER from Mega...");
}

void loop() {
  static String cmd = "";
  while (MegaSerial.available()) {
    char c = MegaSerial.read();
    if (c == '\n') {
      cmd.trim();
      if (cmd == "CHECK") {
        Serial.println("[UART] CHECK received");
        doFaceCheck();
      } else if (cmd.startsWith("REGISTER:")) {   // "REGISTER:<id>"
        String id = cmd.substring(9);
        id.trim();
        Serial.print("[UART] REGISTER received for id "); Serial.println(id);
        doFaceRegister(id);
      }
      cmd = "";
    } else if (cmd == "PING") {   // ← doğru yer: \n geldiğinde, cmd="" öncesi
        MegaSerial.println("PONG");
        Serial.println("[UART] PING -> PONG");
    } else if (c != '\r') {
      cmd += c;
    }
  }
}
```

═══════════════════════════════ KOPYALANACAK KISIM BİTTİ ═══════════════════════════════
