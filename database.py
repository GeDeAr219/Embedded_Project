import sqlite3
import numpy as np


class FaceDatabase:
    def __init__(self, db_path="faces.db"):
        self.conn = sqlite3.connect(db_path)
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