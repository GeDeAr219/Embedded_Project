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
const char* SERVER_CHECK_URL    = "http://172.20.10.4:5000/check";
const char* SERVER_REGISTER_URL = "http://172.20.10.4:5000/register?name=";
const char* SERVER_RESET_URL    = "http://172.20.10.4:5000/reset";
const char* SERVER_LOG_URL      = "http://172.20.10.4:5000/log";
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

// ── Wipe both face database on the PC server ─────────────────
void doDatabaseReset() {
  if (WiFi.status() != WL_CONNECTED) {
    MegaSerial.println("RESET_FAIL");
    Serial.println("[RESET] WiFi down -> RESET_FAIL");
    return;
  }

  HTTPClient http;
  http.begin(SERVER_RESET_URL);
  http.addHeader("Content-Type", "text/plain");
  http.setTimeout(10000);
  int code = http.POST("");
  String payload = (code > 0) ? http.getString() : "";
  http.end();

  Serial.printf("[RESET] HTTP %d  payload='%s'\n", code, payload.c_str());

  if (code == 200 && payload.startsWith("RESET_OK")) {
    MegaSerial.println("RESET_OK");
  } else {
    MegaSerial.println("RESET_FAIL");
  }
}

// ── Forward a Mega access-log line to the PC database ────────
// data is "2026-06-14 14:32:05|GRANTED|1" (RTC time | event | id).
// Fire-and-forget: the Mega does not wait for a reply.
void doLog(const String& data) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LOG] WiFi down -> dropped");
    return;
  }
  HTTPClient http;
  http.begin(SERVER_LOG_URL);
  http.addHeader("Content-Type", "text/plain");
  http.setTimeout(6000);
  int code = http.POST(data);
  http.end();
  Serial.printf("[LOG] POST '%s' -> HTTP %d\n", data.c_str(), code);
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
      } else if (cmd == "RESET_DB") {
        Serial.println("[UART] RESET_DB received");
        doDatabaseReset();
      } else if (cmd.startsWith("LOG|")) {   // "LOG|<ts>|<event>|<id>"
        Serial.print("[UART] LOG received: "); Serial.println(cmd);
        doLog(cmd.substring(4));
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
