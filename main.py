import os
import sys
import cv2
import numpy as np
from insightface.app import FaceAnalysis

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
    def __init__(self, register_dir="register"):
        self.register_dir = register_dir
        self.registered_embeddings = {}
        self.reg_names = []
        self.reg_matrix = np.empty((0, 512))
        self.threshold = 0.60 

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
        self.registered_embeddings.clear()
        if not os.path.exists(self.register_dir):
            os.makedirs(self.register_dir, exist_ok=True)
            
        files = [f for f in os.listdir(self.register_dir) if f.lower().endswith(('.png', '.jpg', '.jpeg'))]
        for file in files:
            img_path = os.path.join(self.register_dir, file)
            try:
                emb = self.get_embedding(img_path)
                self.registered_embeddings[file] = np.array(emb)
            except ValueError:
                print(f"[REJECTED] No face in registration: {file}")
            except Exception as e:
                print(f"[ERROR] Registration failed for {file}: {e}")

        if self.registered_embeddings:
            self.reg_names = list(self.registered_embeddings.keys())
            raw_matrix = np.array(list(self.registered_embeddings.values()))
            self.reg_matrix = raw_matrix / np.linalg.norm(raw_matrix, axis=1, keepdims=True)

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
        if not name.lower().endswith(('.png', '.jpg', '.jpeg')):
            filename = f"{name}.jpg"
        else:
            filename = name
        if not os.path.exists(self.register_dir):
            os.makedirs(self.register_dir, exist_ok=True)
        path = os.path.join(self.register_dir, filename)
        cv2.imwrite(path, img)
        try:
            emb = self.get_embedding(path)
            self.registered_embeddings[filename] = np.array(emb)
            self.reg_names = list(self.registered_embeddings.keys())
            raw_matrix = np.array(list(self.registered_embeddings.values()))
            self.reg_matrix = raw_matrix / np.linalg.norm(raw_matrix, axis=1, keepdims=True)
            return True
        except Exception as e:
            if os.path.exists(path):
                os.remove(path)
            raise e

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
                            ser.write(f"MATCH:{match_name}\n".encode('utf-8'))
                            print(f"MATCH: {match_name} ({dist:.3f})")
                        elif status == "NO_MATCH":
                            ser.write(b"NO_MATCH\n")
                            print("NO_MATCH")
                        else:
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
