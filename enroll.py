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
