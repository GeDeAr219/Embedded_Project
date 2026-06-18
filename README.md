# Biometric Security System

A two-factor biometric access control system using **face recognition** and **fingerprint authentication**.

The system combines an Arduino Mega, ESP32-CAM, R307/AS608 fingerprint sensor, OLED display, relay-controlled door lock, RTC module, LEDs, buzzer, and a Python server.

## Features

- Face recognition with ESP32-CAM and InsightFace
- Fingerprint registration and verification
- Two-factor identity matching
- User registration and database reset
- Door lock control with relay
- OLED status messages
- Alarm, cancel, and silence buttons
- RTC-based access logging
- SQLite database for users and access records

## Technologies

- Arduino Mega 2560
- ESP32-CAM
- Python
- OpenCV
- InsightFace
- SQLite
- HTTP and UART communication

## Project Files

- `FingerprintTest.ino` — Main Arduino Mega control code
- `ESP32CAM_Face.ino` — Camera, Wi-Fi, and server communication
- `main.py` — Face recognition application
- `wifi_server.py` — HTTP API for ESP32-CAM
- `database.py` — SQLite database operations

## Installation

Install the Python dependencies:

```bash
pip install numpy opencv-python insightface onnxruntime
```

Update the Wi-Fi credentials and server IP address in `ESP32CAM_Face.ino`, upload both Arduino sketches, and then start the Python server:

```bash
python main.py
```

## How It Works

1. The ESP32-CAM captures and verifies the user's face.
2. The Arduino Mega requests fingerprint verification.
3. Access is granted only when the face ID and fingerprint ID match.
4. The relay unlocks the door and the event is saved to the SQLite database.
