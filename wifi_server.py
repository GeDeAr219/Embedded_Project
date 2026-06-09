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