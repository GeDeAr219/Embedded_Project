"""
Quick end-to-end face-check test (no Mega needed).
Grabs a single still from the ESP32-CAM and POSTs it to the PC face
server's /check endpoint, then prints the result.

Run main.py first (so the server is up with enrolled faces loaded).
Usage:  python test_check.py
"""
import urllib.request

ESP_CAPTURE = "http://172.20.10.3/capture"      # ESP32-CAM single still
SERVER_CHECK = "http://172.20.10.4:8080/check"  # PC face server

print(f"[TEST] Grabbing frame from {ESP_CAPTURE} ...")
img = urllib.request.urlopen(ESP_CAPTURE, timeout=10).read()
print(f"[TEST] Got {len(img)} bytes. Sending to {SERVER_CHECK} ...")

req = urllib.request.Request(SERVER_CHECK, data=img,
                             headers={"Content-Type": "image/jpeg"})
resp = urllib.request.urlopen(req, timeout=20).read().decode("utf-8", "ignore")
print(f"[TEST] Server replied: {resp}")
