import cv2
import sys
import numpy as np
import urllib.request
from database import FaceDatabase
from main import FaceRecognizer

# ESP32-CAM's IP address
ESP32_STREAM_URL = "http://192.168.1.XXX/stream"


def enroll_from_esp32(recognizer, name, stream_url=ESP32_STREAM_URL):
    print(f"[ENROLL] Connecting to ESP32-CAM: {stream_url}")
    print(f"[ENROLL] Enrolling: '{name}'")
    print("Press SPACE to capture, ESC to cancel.\n")

    stream = urllib.request.urlopen(stream_url, timeout=10)
    bytes_buffer = b""

    while True:
        bytes_buffer += stream.read(4096)
        start = bytes_buffer.find(b'\xff\xd8')
        end   = bytes_buffer.find(b'\xff\xd9')

        if start != -1 and end != -1:
            jpg = bytes_buffer[start:end+2]
            bytes_buffer = bytes_buffer[end+2:]

            nparr = np.frombuffer(jpg, np.uint8)
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            if frame is None:
                continue

            # live preview
            preview = frame.copy()
            cv2.putText(preview, f"Enrolling: {name}", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
            cv2.putText(preview, "SPACE: capture | ESC: cancel", (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            cv2.imshow("ESP32-CAM Enroll", preview)

            key = cv2.waitKey(1) & 0xFF

            if key == 27:  # ESC
                print("[ENROLL] Cancelled.")
                cv2.destroyAllWindows()
                return False

            if key == 32:  # SPACE
                try:
                    recognizer.register_face(frame, name)
                    print(f"[ENROLL] Successfully enrolled: '{name}'")
                    cv2.destroyAllWindows()
                    return True
                except ValueError as e:
                    print(f"[ENROLL] No face detected, try again: {e}")


def enroll_from_image(recognizer, name, img_path):
    img = cv2.imread(img_path)
    if img is None:
        print(f"[ERROR] Cannot read image: {img_path}")
        return False
    try:
        recognizer.register_face(img, name)
        print(f"[ENROLL] Enrolled '{name}' from {img_path}")
        return True
    except ValueError as e:
        print(f"[ENROLL] Failed: {e}")
        return False


def list_users(db):
    users = db.list_users()
    if not users:
        print("No users registered.")
        return
    print("\n---- Registered Users ----")
    for uid, name in users:
        print(f"  ID {uid:3d} | {name}")
    print(f"  Total: {len(users)}\n")


def delete_user(db):
    list_users(db)
    try:
        uid = int(input("Enter user ID to delete (0 to cancel): "))
        if uid == 0:
            return
        if db.delete_user(uid):
            print(f"[DELETE] User {uid} removed.")
        else:
            print(f"[DELETE] ID {uid} not found.")
    except ValueError:
        print("[ERROR] Invalid input.")


def interactive_menu(recognizer):
    db = recognizer.db
    stream_url = ESP32_STREAM_URL

    while True:
        print("\n==== ADMIN ENROLLMENT MENU ====")
        print(f"  ESP32-CAM: {stream_url}")
        print("  1. Enroll from ESP32-CAM")
        print("  2. Enroll from image file")
        print("  3. List registered users")
        print("  4. Delete a user")
        print("  5. Change ESP32-CAM IP")
        print("  6. Exit")
        print("================================")
        choice = input("Select: ").strip()

        if choice == "1":
            name = input("Enter name: ").strip()
            if name:
                enroll_from_esp32(recognizer, name, stream_url)
        elif choice == "2":
            name = input("Enter name: ").strip()
            path = input("Image path: ").strip()
            if name and path:
                enroll_from_image(recognizer, name, path)
        elif choice == "3":
            list_users(db)
        elif choice == "4":
            delete_user(db)
        elif choice == "5":
            ip = input("Enter ESP32-CAM IP (e.g. 192.168.1.42): ").strip()
            stream_url = f"http://{ip}/stream"
            print(f"Updated: {stream_url}")
        elif choice == "6":
            break
        else:
            print("Invalid choice.")


if __name__ == "__main__":
    recognizer = FaceRecognizer()
    recognizer.load_registered_faces()

    # python enroll.py "Emirhan"         → direct camera enroll
    if len(sys.argv) == 2:
        enroll_from_esp32(recognizer, sys.argv[1])

    # python enroll.py "Emirhan" photo.jpg  → enroll from image
    elif len(sys.argv) == 3:
        enroll_from_image(recognizer, sys.argv[1], sys.argv[2])

    # python enroll.py                   → interactive menu
    else:
        interactive_menu(recognizer)