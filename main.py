import os
import sys
import numpy as np
from insightface.app import FaceAnalysis
from database import FaceDatabase


class SuppressOutput:
    """Silence the noisy InsightFace/ONNX startup logs."""
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
            self.app = FaceAnalysis(
                name='buffalo_s',
                providers=['CUDAExecutionProvider', 'CPUExecutionProvider'])
            self.app.prepare(ctx_id=0, det_size=(320, 320))

    def load_registered_faces(self):
        # Load all users (names + embeddings) from the DB into RAM.
        self.reg_names, raw_matrix = self.db.load_users()
        if len(self.reg_names) > 0:
            self.reg_matrix = raw_matrix / np.linalg.norm(raw_matrix, axis=1, keepdims=True)
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

    def register_face(self, img, name):
        # Detect a face, take the first embedding, store it, refresh RAM.
        faces = self.app.get(img)
        if not faces:
            raise ValueError("No face detected.")
        emb = faces[0].embedding
        self.db.add_user(name, np.array(emb))
        self.load_registered_faces()
        return True


if __name__ == "__main__":
    import time

    recognizer = FaceRecognizer()
    recognizer.load_registered_faces()

    from wifi_server import start_wifi_server
    start_wifi_server(recognizer, port=5000)

    print("[SYS] WiFi server running. Press Ctrl+C to stop.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("[SYS] Shutting down.")
